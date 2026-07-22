#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${RUNTIME_STRESS_BUILD_DIR:-$ROOT_DIR/build-runtime-stress}"
LOG="${RUNTIME_STRESS_LOG:-$BUILD_DIR/qemu-runtime-stress.log}"
MONITOR="${RUNTIME_STRESS_MONITOR:-$BUILD_DIR/qemu-monitor.sock}"
TIMEOUT="${QEMU_TEST_TIMEOUT:-25s}"
INJECT_SECONDS="${RUNTIME_STRESS_INJECT_SECONDS:-12}"
QEMU="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    if [[ -f "$LOG" ]]; then
        cat "$LOG" >&2 || true
    fi
    exit 1
}

require_marker() {
    local marker="$1"
    grep -Fq "$marker" "$LOG" || fail "missing marker: $marker"
}

command -v timeout >/dev/null 2>&1 || fail "required command not found: timeout"
command -v python3 >/dev/null 2>&1 || fail "required command not found: python3"
command -v "$QEMU" >/dev/null 2>&1 || fail "required command not found: $QEMU"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# The production tree is not modified by these flags. PANEL_AUTO_TEST creates
# real GUI/redraw churn, while ARMONIOS_RUNTIME_STRESS_TEST adds serial-only
# evidence and forces one cooperative deadline expiry every eight service passes.
make -C "$ROOT_DIR" \
    BUILD_DIR="$BUILD_DIR" \
    BOARD_CFLAGS="-DARMONIOS_RUNTIME_STRESS_TEST" \
    USERLAND_EXTRA_CFLAGS="-DPANEL_AUTO_TEST" \
    >/dev/null

[[ -f "$BUILD_DIR/kernel.bin" ]] || fail "stress kernel was not built"
rm -f "$LOG" "$MONITOR"

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
    -netdev user,id=net0 \
    -device virtio-net-device,netdev=net0 \
    >/dev/null 2>&1 &
qemu_pid=$!

# Feed real keyboard events through QEMU's monitor while the panel and launched
# applications keep yielding and redrawing. The monitor output is drained so its
# socket cannot back-pressure QEMU during the test.
python3 - "$MONITOR" "$INJECT_SECONDS" <<'PY' || {
import socket
import sys
import time

path = sys.argv[1]
duration = float(sys.argv[2])
deadline = time.monotonic() + 10.0
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
while True:
    try:
        sock.connect(path)
        break
    except (FileNotFoundError, ConnectionRefusedError):
        if time.monotonic() >= deadline:
            raise
        time.sleep(0.05)

sock.setblocking(False)
end = time.monotonic() + duration
keys = (b"a", b"b", b"c", b"d")
i = 0
while time.monotonic() < end:
    try:
        sock.sendall(b"sendkey " + keys[i % len(keys)] + b" 30\n")
    except (BrokenPipeError, ConnectionResetError):
        break
    i += 1
    try:
        while sock.recv(4096):
            pass
    except BlockingIOError:
        pass
    time.sleep(0.08)
sock.close()
PY
    kill "$qemu_pid" >/dev/null 2>&1 || true
    wait "$qemu_pid" >/dev/null 2>&1 || true
    fail "keyboard injection failed"
}

wait "$qemu_pid" || status=$?
if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
    fail "QEMU exited with status $status"
fi
[[ -s "$LOG" ]] || fail "QEMU produced no serial log"

if grep -Fq "__PANIC_HALT__" "$LOG"; then
    fail "kernel panic detected"
fi
if grep -Fq "runtime-stress: input overflow" "$LOG"; then
    fail "observable input queue overflow occurred"
fi

# Do not use ordinary boot strings as stress gates. IRQ diagnostics can legally
# interleave with EL0 serial writes character-by-character. The markers below are
# emitted by the actual work paths and the heartbeat proves EL0 executed.
require_marker "[net] DHCP ack:"
require_marker "runtime-stress: input consumed"
require_marker "runtime-stress: redraw submitted"
require_marker "runtime-stress: network frame"

heartbeat_count=$(grep -Fc "runtime-stress: EL0 heartbeat" "$LOG" || true)
deadline_count=$(grep -Fc "runtime-stress: deadline republished" "$LOG" || true)

(( heartbeat_count >= 3 )) || fail "expected at least 3 EL0 heartbeats, found $heartbeat_count"
(( deadline_count >= 3 )) || fail "expected at least 3 deadline expirations, found $deadline_count"

printf 'PASS: runtime stress: %d EL0 heartbeats, %d deadline expirations\n' \
    "$heartbeat_count" "$deadline_count"
printf 'PASS: input, redraw, and network progress observed without input overflow\n'
printf 'PASS: qemu-runtime-stress log %s\n' "$LOG"
