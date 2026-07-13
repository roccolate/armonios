#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${USERCOPY_BUILD_DIR:-$ROOT_DIR/build-usercopy-test}"
KERNEL_BIN="${USERCOPY_KERNEL_BIN:-$BUILD_DIR/kernel.bin}"
LOG="${USERCOPY_TEST_LOG:-$BUILD_DIR/qemu-usercopy-test.log}"
TIMEOUT="${USERCOPY_TEST_TIMEOUT:-30s}"
QEMU="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"

fail() {
    printf 'FAIL: %s\n' "$*" >&2
    if [[ -f "$LOG" ]]; then
        cat "$LOG" >&2 || true
    fi
    exit 1
}

if [[ "${USERCOPY_SKIP_BUILD:-0}" != "1" ]]; then
    make -C "$ROOT_DIR" \
        BUILD_DIR="$BUILD_DIR" \
        USERLAND_EXTRA_CFLAGS="-DPANEL_AUTO_TEST" \
        USERLAND_ASFLAGS="-Wall -Wextra -ffreestanding -nostdlib -nostartfiles -mcpu=cortex-a72 -g -I programs -I programs/libkarm -DUSERCOPY_RX_PROBE"
fi

command -v timeout >/dev/null 2>&1 || fail "required command not found: timeout"
command -v "$QEMU" >/dev/null 2>&1 || fail "required command not found: $QEMU"
[[ -f "$KERNEL_BIN" ]] || fail "kernel image not found: $KERNEL_BIN"

mkdir -p "$(dirname "$LOG")"
rm -f "$LOG"
status=0

timeout "$TIMEOUT" "$QEMU" \
    -machine virt -cpu cortex-a72 -m 128M \
    -display none -serial "file:$LOG" -monitor none \
    -global virtio-mmio.force-legacy=false \
    -kernel "$KERNEL_BIN" \
    -device virtio-gpu-device,xres=640,yres=480 \
    >/dev/null 2>&1 || status=$?

if [[ "$status" -ne 0 && "$status" -ne 124 ]]; then
    fail "QEMU exited with status $status"
fi

[[ -s "$LOG" ]] || fail "QEMU produced no serial log"

if grep -Fq "USERCOPY: unexpected result" "$LOG"; then
    fail "EL0 RX destination was not rejected"
fi

probe_count="$(grep -Fc "USERCOPY: RX output rejected" "$LOG" || true)"
if [[ "$probe_count" -lt 2 ]]; then
    fail "expected at least two successful EL0 probes, found $probe_count"
fi

grep -Fq "panel: ready" "$LOG" || fail "panel did not remain alive after probe"
grep -Fq "clock: starting" "$LOG" || fail "a second process was not scheduled"

printf 'PASS: EL0 RX output rejected in %s processes\n' "$probe_count"
printf 'PASS: panel survived and scheduled clock\n'
printf 'qemu-usercopy-test: log %s\n' "$LOG"
