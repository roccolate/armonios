#!/usr/bin/env bash

# Build a dedicated test kernel whose first EL0 image is loaded from
# /fat/HELLO.KLI, then require the SDK application's real serial output.
# The production kernel and normal panel boot path are not modified by this run.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests/external-kli-runtime"
TEST_BUILD="$ROOT_DIR/build-external-kli-runtime"
FAT_IMAGE="$OUT_DIR/external-kli.img"
SERIAL_LOG="$OUT_DIR/serial.log"
TIMEOUT="${QEMU_EXTERNAL_KLI_TIMEOUT:-25s}"

rm -rf "$OUT_DIR" "$TEST_BUILD"
mkdir -p "$OUT_DIR"

make -C "$ROOT_DIR" external-kli-image EXTERNAL_KLI_IMG="$FAT_IMAGE"
make -C "$ROOT_DIR" BOARD=qemu_virt BUILD_DIR="$TEST_BUILD" \
    BOARD_CFLAGS=-DARMONIOS_EXTERNAL_KLI_BOOT_TEST=1

status=0
timeout "$TIMEOUT" qemu-system-aarch64 \
    -machine virt -cpu cortex-a72 -m 128M \
    -display none -serial file:"$SERIAL_LOG" -monitor none \
    -no-reboot \
    -global virtio-mmio.force-legacy=false \
    -kernel "$TEST_BUILD/kernel.bin" \
    -drive file="$FAT_IMAGE",if=none,format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    >/dev/null 2>&1 || status=$?

if [[ $status -ne 0 && $status -ne 124 ]]; then
    cat "$SERIAL_LOG" >&2 || true
    exit "$status"
fi
if grep -q '__PANIC_HALT__' "$SERIAL_LOG"; then
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: external KLI runtime test hit a kernel panic' >&2
    exit 1
fi

grep -q 'FAT32: mounted' "$SERIAL_LOG" || {
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: FAT32 did not mount in external KLI runtime test' >&2
    exit 1
}
grep -q 'Hello from an external ArmoniOS SDK app' "$SERIAL_LOG" || {
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: SDK-built external KLI did not execute in EL0' >&2
    exit 1
}
grep -q 'USER exit: 0x0000000000000000' "$SERIAL_LOG" || {
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: external KLI did not exit cleanly' >&2
    exit 1
}
grep -q 'panel:exit' "$SERIAL_LOG" || {
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: recovery wrapper did not observe the EL0 return' >&2
    exit 1
}
grep -q 'USER exit code: 0x0000000000000000' "$SERIAL_LOG" || {
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: kernel did not regain control after external KLI exit' >&2
    exit 1
}

printf 'PASS: SDK-built HELLO.KLI executed from FAT32 in EL0 and returned cleanly\n'
printf 'QEMU log: %s\n' "$SERIAL_LOG"
