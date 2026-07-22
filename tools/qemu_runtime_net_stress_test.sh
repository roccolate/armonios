#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${RUNTIME_NET_STRESS_BUILD_DIR:-$ROOT_DIR/build-runtime-net-stress}"
LOG="${RUNTIME_NET_STRESS_LOG:-$BUILD_DIR/qemu-runtime-net-stress.log}"
BUILD_LOG="${RUNTIME_NET_STRESS_BUILD_LOG:-$BUILD_DIR/build.log}"
QEMU_LOG="${RUNTIME_NET_STRESS_QEMU_LOG:-$BUILD_DIR/qemu-stderr.log}"
MONITOR="${RUNTIME_NET_STRESS_MONITOR:-$BUILD_DIR/qemu-monitor.sock}"
TIMEOUT="${QEMU_TEST_TIMEOUT:-30s}"
LOAD_SECONDS="${RUNTIME_NET_STRESS_SECONDS:-12}"
HOST_PORT="${RUNTIME_NET_STRESS_HOST_PORT:-45555}"
QEMU="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    for diagnostic in "$BUILD_LOG" "$QEMU_LOG" "$LOG"; do
        if [[ -f "$diagnostic" ]]; then
            printf '%s\n' "--- $diagnostic" >&2
            cat "$diagnostic" >&2 || true
        fi
    done
    exit 1
}

extract_field() {
    local line="$1"
    local field="$2"
    printf '%s\n' "$line" | sed -n "s/.* ${field}=\([0-9][0-9]*\).*/\1/p"
}

command -v timeout >/dev/null 2>&1 || fail "required command not found: timeout"
command -v python3 >/dev/null 2>&1 || fail "required command not found: python3"
command -v "$QEMU" >/dev/null 2>&1 || fail "required command not found: $QEMU"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# This image keeps the production deadline untouched. It adds only EL0-side
# telemetry summaries; no forced expiration or shortened budget is used.
if ! make -C "$ROOT_DIR" \
    BUILD_DIR="$BUILD_DIR" \
    BOARD_CFLAGS="-DARMONIOS_RUNTIME_NET_STRESS_TEST" \
    USERLAND_EXTRA_CFLAGS="-DPANEL_AUTO_TEST" \
    >"$BUILD_LOG" 2>&1; then
    fail "network stress image build failed"
fi

[[ -f "$BUILD_DIR/kernel.bin" ]] || fail "network stress kernel was not built"
printf 'instrumented kernel bytes: %s\n' "$(wc -c < "$BUILD_DIR/kernel.bin")" >>"$BUILD_LOG"
rm -f "$LOG" "$QEMU_LOG" "$MONITOR"

status=0
timeout "$TIMEOUT" "$QEMU" \
    -machine virt -cpu cortex-a72 -m 128M \
    -display none -serial "file:$LOG" \
    -monitor "unix:$MONITOR,server=on,wait=off" \
    -no-reboot \
    -global virtio-mmio.force-legacy=false \
    -kernel "$BUILD_DIR/kernel.bin" \
    -device virtio-gpu-device,xres=640,yres=480 \
    -device qemu-xhci,id=xhci \
    -device usb-kbd,bus=xhci.0 \
    -device usb-mouse,bus=xhci.0 \
    -netdev "user,id=net0,hostfwd=udp:127.0.0.1:${HOST_PORT}-:5555" \
    -device virtio-net-device,netdev=net0 \
    >"$QEMU_LOG" 2>&1 &
qemu_pid=$!

# Wait for DHCP, then keep the 16-descriptor RX ring under pressure while real
# USB keyboard events and panel redraw work continue. UDP port 5555 needs no
# guest socket: the minimal stack still receives and accounts each Ethernet frame
# before discarding non-DHCP payloads.
python3 - "$MONITOR" "$LOG" "$LOAD_SECONDS" "$HOST_PORT" <<'PY' || {
import socket
import sys
import time

monitor_path = sys.argv[1]
log_path = sys.argv[2]
duration = float(sys.argv[3])
host_port = int(sys.argv[4])

connect_deadline = time.monotonic() + 10.0
monitor = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
while True:
    try:
        monitor.connect(monitor_path)
        break
    except (FileNotFoundError, ConnectionRefusedError):
        if time.monotonic() >= connect_deadline:
            raise
        time.sleep(0.05)
monitor.setblocking(False)

boot_deadline = time.monotonic() + 12.0
while True:
    try:
        with open(log_path, "r", encoding="utf-8", errors="ignore") as stream:
            if "[net] DHCP ack:" in stream.read():
                break
    except FileNotFoundError:
        pass
    if time.monotonic() >= boot_deadline:
        raise RuntimeError("DHCP acknowledgement did not arrive")
    time.sleep(0.05)

udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
payload = b"ArmoniOS-runtime-net-stress" * 8
keys = (b"a", b"b", b"c", b"d")
key_index = 0
next_key = time.monotonic()
end = time.monotonic() + duration
sent = 0

while time.monotonic() < end:
    for _ in range(64):
        udp.sendto(payload, ("127.0.0.1", host_port))
        sent += 1

    now = time.monotonic()
    if now >= next_key:
        monitor.sendall(b"sendkey " + keys[key_index % len(keys)] + b" 30\n")
        key_index += 1
        next_key = now + 0.08

    try:
        while monitor.recv(4096):
            pass
    except BlockingIOError:
        pass
    time.sleep(0.002)

udp.close()
time.sleep(1.0)
monitor.sendall(b"quit\n")
monitor.close()
print(f"host UDP frames submitted: {sent}")
PY
    kill "$qemu_pid" >/dev/null 2>&1 || true
    wait "$qemu_pid" >/dev/null 2>&1 || true
    fail "network and keyboard injection failed"
}

wait "$qemu_pid" || status=$?
if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
    fail "QEMU exited with status $status"
fi
[[ -s "$LOG" ]] || fail "QEMU produced no serial log"

if grep -Fq "__PANIC_HALT__" "$LOG"; then
    fail "kernel panic detected"
fi

grep -Fq "[net] DHCP ack:" "$LOG" || fail "missing DHCP acknowledgement"

summary_count=$(grep -c '^runtime-net-stress: summary ' "$LOG" || true)
(( summary_count >= 3 )) || fail "expected at least 3 atomic EL0 summaries, found $summary_count"

summary=$(grep '^runtime-net-stress: summary ' "$LOG" | tail -1)
yields=$(extract_field "$summary" yields)
input=$(extract_field "$summary" input)
redraw=$(extract_field "$summary" redraw)
frames=$(extract_field "$summary" frames)
netmax=$(extract_field "$summary" netmax)
netcap=$(extract_field "$summary" netcap)
over=$(extract_field "$summary" over)
max_ticks=$(extract_field "$summary" max)
budget_ticks=$(extract_field "$summary" budget)
requeue=$(extract_field "$summary" requeue)
overflow=$(extract_field "$summary" overflow)

for value in "$yields" "$input" "$redraw" "$frames" "$netmax" "$netcap" \
             "$over" "$max_ticks" "$budget_ticks" "$requeue" "$overflow"; do
    [[ -n "$value" ]] || fail "could not parse final telemetry summary: $summary"
done

(( yields >= 3072 )) || fail "insufficient EL0 progress: $yields yields"
(( input > 0 )) || fail "no input consumption observed"
(( redraw > 0 )) || fail "no redraw submission observed"
(( frames >= 128 )) || fail "insufficient received network frames: $frames"
(( netmax == 16 )) || fail "RX pass never reached the 16-frame cap: max=$netmax"
(( netcap >= 4 )) || fail "insufficient repeated RX saturation: $netcap exhaustions"
(( requeue >= netcap )) || fail "network continuation was not reflected in requeues"
(( budget_ticks > 0 )) || fail "runtime deadline budget was not configured"
(( max_ticks > 0 )) || fail "runtime duration was not measured"
(( overflow == 0 )) || fail "observable input queue overflow: $overflow"

ratio=$(( max_ticks * 100 / budget_ticks ))
printf 'PASS: runtime net stress: %s frames, %s capped passes, %s EL0 yields\n' \
    "$frames" "$netcap" "$yields"
printf 'PASS: natural max duration %s / %s ticks (%s%%), overruns %s\n' \
    "$max_ticks" "$budget_ticks" "$ratio" "$over"
printf 'PASS: input=%s redraw=%s requeue=%s overflow=%s\n' \
    "$input" "$redraw" "$requeue" "$overflow"
printf 'PASS: qemu-runtime-net-stress log %s\n' "$LOG"
