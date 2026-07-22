#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KERNEL_BUILD_DIR="${VMM_SOAK_KERNEL_BUILD_DIR:-$ROOT_DIR/build}"
OUTPUT_DIR="${VMM_SOAK_OUTPUT_DIR:-$ROOT_DIR/build-vmm-soak}"
BOOT_COUNT="${VMM_SOAK_BOOT_COUNT:-10}"
BOOT_SECONDS="${VMM_SOAK_BOOT_SECONDS:-10}"
GDB_PORT_BASE="${VMM_SOAK_GDB_PORT_BASE:-24600}"
QEMU="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"
GDB="${GDB_MULTIARCH:-gdb-multiarch}"
ADDR2LINE="${AARCH64_ADDR2LINE:-aarch64-linux-gnu-addr2line}"
METADATA_LOG="$OUTPUT_DIR/metadata.log"
KERNEL_ELF="$KERNEL_BUILD_DIR/kernel.elf"
KERNEL_BIN="$KERNEL_BUILD_DIR/kernel.bin"
BLOCK_IMAGE="$KERNEL_BUILD_DIR/virtio-blk.img"

current_pid=""
current_monitor=""
current_serial=""
current_qemu_log=""
current_gdb_log=""

cleanup() {
    if [[ -n "$current_pid" ]]; then
        kill "$current_pid" >/dev/null 2>&1 || true
        wait "$current_pid" >/dev/null 2>&1 || true
    fi
    rm -f "$current_monitor"
}
trap cleanup EXIT

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    for diagnostic in "$METADATA_LOG" "$current_qemu_log" \
                      "$current_serial" "$current_gdb_log"; do
        if [[ -n "$diagnostic" && -f "$diagnostic" ]]; then
            printf '%s\n' "--- $diagnostic" >&2
            tail -240 "$diagnostic" >&2 || true
        fi
    done
    exit 1
}

for command in python3 "$QEMU" "$GDB" "$ADDR2LINE"; do
    command -v "$command" >/dev/null 2>&1 || \
        fail "required command not found: $command"
done

[[ "$BOOT_COUNT" =~ ^[1-9][0-9]*$ ]] || fail "invalid boot count: $BOOT_COUNT"
[[ "$BOOT_SECONDS" =~ ^[1-9][0-9]*$ ]] || fail "invalid boot duration: $BOOT_SECONDS"

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

# tools/verify.sh runs the ordinary build and qemu-fs-test immediately before
# this gate. Reuse those exact production artifacts instead of rebuilding a
# second layout. Standalone invocations build only when an artifact is missing.
if [[ ! -f "$KERNEL_ELF" || ! -f "$KERNEL_BIN" || ! -f "$BLOCK_IMAGE" ]]; then
    if ! make -C "$ROOT_DIR" \
        BUILD_DIR="$KERNEL_BUILD_DIR" \
        all "$BLOCK_IMAGE" \
        >"$METADATA_LOG" 2>&1; then
        fail "production FAT32 artifacts could not be built"
    fi
fi

[[ -f "$KERNEL_ELF" ]] || fail "missing kernel ELF: $KERNEL_ELF"
[[ -f "$KERNEL_BIN" ]] || fail "missing kernel image: $KERNEL_BIN"
[[ -f "$BLOCK_IMAGE" ]] || fail "missing FAT32 block image: $BLOCK_IMAGE"

{
    printf 'commit: %s\n' "$(git -C "$ROOT_DIR" rev-parse --verify HEAD 2>/dev/null || printf unknown)"
    printf 'kernel build directory: %s\n' "$KERNEL_BUILD_DIR"
    printf 'kernel sha256: %s\n' "$(sha256sum "$KERNEL_BIN" | awk '{print $1}')"
    printf 'kernel bytes: %s\n' "$(wc -c < "$KERNEL_BIN")"
    printf 'block image sha256: %s\n' "$(sha256sum "$BLOCK_IMAGE" | awk '{print $1}')"
    printf 'qemu: %s\n' "$($QEMU --version | head -1)"
    printf 'boots: %s, seconds/boot: %s\n' "$BOOT_COUNT" "$BOOT_SECONDS"
} >"$METADATA_LOG"

capture_gdb() {
    local port="$1"
    local output="$2"
    local commands="$OUTPUT_DIR/gdb-commands.txt"

    cat >"$commands" <<'GDB'
set pagination off
set confirm off
info registers
info all-registers
bt
p/x g_current_process
p/x &g_current_process
x/gx &g_current_process
set $current = g_current_process
if $current != 0
  p *g_current_process
end
p/x g_mem_base
p/x g_total_pages
p/x g_free_pages
p/x &g_bitmap
x/16gx &g_bitmap
p/x $ttbr0_el1
p/x $ttbr1_el1
p/x $tcr_el1
p/x $sctlr_el1
p/x $esr_el1
p/x $far_el1
p/x $elr_el1
disassemble /r next_table
GDB

    "$GDB" -q -batch \
        -ex "file $KERNEL_ELF" \
        -ex "target remote 127.0.0.1:$port" \
        -x "$commands" \
        >"$output" 2>&1 || true
}

quit_monitor() {
    local monitor="$1"
    python3 - "$monitor" <<'PY' || true
import socket
import sys
import time

path = sys.argv[1]
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
for _ in range(100):
    try:
        sock.connect(path)
        sock.sendall(b"quit\n")
        sock.close()
        break
    except (FileNotFoundError, ConnectionRefusedError, BrokenPipeError):
        time.sleep(0.02)
PY
}

for ((iteration = 1; iteration <= BOOT_COUNT; iteration++)); do
    label=$(printf '%02d' "$iteration")
    serial_log="$OUTPUT_DIR/boot-$label.log"
    qemu_log="$OUTPUT_DIR/qemu-$label.stderr.log"
    monitor="$OUTPUT_DIR/monitor-$label.sock"
    gdb_log="$OUTPUT_DIR/gdb-$label.log"
    addr_log="$OUTPUT_DIR/addr2line-$label.log"
    port=$((GDB_PORT_BASE + iteration))

    rm -f "$serial_log" "$qemu_log" "$monitor" "$gdb_log" "$addr_log"
    current_monitor="$monitor"
    current_serial="$serial_log"
    current_qemu_log="$qemu_log"
    current_gdb_log="$gdb_log"

    "$QEMU" \
        -machine virt -cpu cortex-a72 -m 128M \
        -display none -serial "file:$serial_log" \
        -monitor "unix:$monitor,server=on,wait=off" \
        -gdb "tcp:127.0.0.1:$port" \
        -no-reboot \
        -global virtio-mmio.force-legacy=false \
        -kernel "$KERNEL_BIN" \
        -drive "file=$BLOCK_IMAGE,if=none,format=raw,id=hd0" \
        -device virtio-blk-device,drive=hd0 \
        >"$qemu_log" 2>&1 &
    current_pid=$!

    monitor_status=0
    python3 - "$monitor" "$serial_log" "$BOOT_SECONDS" "$current_pid" <<'PY' || monitor_status=$?
import os
import socket
import sys
import time

monitor_path = sys.argv[1]
serial_path = sys.argv[2]
duration = float(sys.argv[3])
qemu_pid = int(sys.argv[4])

connect_deadline = time.monotonic() + 8.0
monitor = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
while True:
    try:
        monitor.connect(monitor_path)
        break
    except (FileNotFoundError, ConnectionRefusedError):
        if time.monotonic() >= connect_deadline:
            raise
        time.sleep(0.03)

end = time.monotonic() + duration
while time.monotonic() < end:
    try:
        with open(serial_path, "r", encoding="utf-8", errors="ignore") as stream:
            if "__PANIC_HALT__" in stream.read():
                monitor.close()
                raise SystemExit(42)
    except FileNotFoundError:
        pass
    try:
        os.kill(qemu_pid, 0)
    except ProcessLookupError:
        monitor.close()
        raise SystemExit(43)
    time.sleep(0.04)

monitor.sendall(b"quit\n")
monitor.close()
PY

    if [[ "$monitor_status" -eq 42 ]]; then
        printf 'panic detected in boot %s; capturing live guest state\n' "$label" >&2
        capture_gdb "$port" "$gdb_log"
        elr=$(grep -E 'ELR_EL1:' "$serial_log" | tail -1 | grep -oE '0x[0-9a-fA-F]+' || true)
        if [[ -n "$elr" ]]; then
            "$ADDR2LINE" -e "$KERNEL_ELF" -f -C "$elr" >"$addr_log" 2>&1 || true
        fi
        quit_monitor "$monitor"
        wait "$current_pid" >/dev/null 2>&1 || true
        current_pid=""
        fail "EL1 panic reproduced on FAT32 soak boot $iteration/$BOOT_COUNT"
    elif [[ "$monitor_status" -ne 0 ]]; then
        quit_monitor "$monitor"
        wait "$current_pid" >/dev/null 2>&1 || true
        current_pid=""
        fail "QEMU or monitor failed on boot $iteration/$BOOT_COUNT (status $monitor_status)"
    fi

    qemu_status=0
    wait "$current_pid" || qemu_status=$?
    current_pid=""
    current_monitor=""

    if [[ "$qemu_status" -ne 0 ]]; then
        fail "QEMU exited with status $qemu_status on boot $iteration/$BOOT_COUNT"
    fi
    [[ -s "$serial_log" ]] || fail "boot $iteration produced no serial log"
    grep -Fq "storage: initialized" "$serial_log" || \
        fail "boot $iteration did not initialize storage"
    grep -Fq "FAT32 root: mounted" "$serial_log" || \
        fail "boot $iteration did not mount the FAT32 root"
    grep -Fq "panel:" "$serial_log" || \
        fail "boot $iteration did not reach the panel path"
    if grep -Fq "__PANIC_HALT__" "$serial_log"; then
        fail "boot $iteration recorded a late panic"
    fi

    printf 'PASS: FAT32 VMM soak boot %d/%d\n' "$iteration" "$BOOT_COUNT"
done

printf 'PASS: %s repeated production FAT32 boots completed without EL1 panic\n' \
    "$BOOT_COUNT"
printf 'PASS: VMM soak artifacts %s\n' "$OUTPUT_DIR"
