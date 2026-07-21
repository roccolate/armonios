#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-el2-entry-test}"
KERNEL_BIN="$BUILD_DIR/kernel.bin"
LOG="$BUILD_DIR/qemu-el2-entry-test.log"
TIMEOUT="${QEMU_EL2_TEST_TIMEOUT:-25s}"
QEMU="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    if [[ -f "$LOG" ]]; then
        cat "$LOG" >&2 || true
    fi
    exit 1
}

command -v timeout >/dev/null 2>&1 || fail "required command not found: timeout"
command -v "$QEMU" >/dev/null 2>&1 || fail "required command not found: $QEMU"

rm -rf "$BUILD_DIR"
make -C "$ROOT_DIR" BOARD=qemu_virt BUILD_DIR="$BUILD_DIR"

status=0
timeout "$TIMEOUT" "$QEMU" \
    -machine virt,virtualization=on -cpu cortex-a72 -m 128M \
    -display none -serial "file:$LOG" -monitor none -no-reboot \
    -global virtio-mmio.force-legacy=false \
    -kernel "$KERNEL_BIN" \
    -device virtio-gpu-device,xres=640,yres=480 \
    >/dev/null 2>&1 || status=$?

if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
    fail "EL2-entry QEMU exited with status $status"
fi
[[ -s "$LOG" ]] || fail "EL2-entry QEMU produced no serial log"
if grep -Fq "__PANIC_HALT__" "$LOG"; then
    fail "kernel panic detected after EL2 entry"
fi
grep -Fq "MMU: active" "$LOG" || fail "EL1 MMU marker missing"
grep -Fq "display: windows" "$LOG" || fail "display marker missing"
grep -Fq "panel: ready" "$LOG" || fail "EL0 panel marker missing"

printf 'PASS: QEMU EL2 -> EL1 entry (%s)\n' "$LOG"
