#!/usr/bin/env bash

# Build a dedicated external parent KLI, place it in FAT32, and boot a dedicated
# test kernel whose first EL0 image comes from /fat/HELLO.KLI. The parent then
# invokes SYS_SPAWN on the same VFS path, waits for the child, and exits.
# Production builds compile neither test mode.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests/external-kli-runtime"
TEST_BUILD="$ROOT_DIR/build-external-kli-runtime"
SDK_DIR="$ROOT_DIR/build/sdk"
SDK_EXAMPLE="$SDK_DIR/examples/hello-console"
HELLO_KLI="$SDK_EXAMPLE/build/HELLO.KLI"
SHELL_KLI="$ROOT_DIR/build/programs/apps/shell.bin"
FAT_IMAGE="$OUT_DIR/external-kli.img"
MKFAT="$OUT_DIR/mkfat32_image"
SERIAL_LOG="$OUT_DIR/serial.log"
HOST_CC="${HOST_CC:-cc}"
TIMEOUT="${QEMU_EXTERNAL_KLI_TIMEOUT:-25s}"

rm -rf "$OUT_DIR" "$TEST_BUILD"
mkdir -p "$OUT_DIR"

# Build the normal QEMU tree and assemble a fresh SDK copy. Then explicitly
# clean and rebuild the external example with the spawn-test macro so no stale
# HELLO.KLI can satisfy the target through timestamps.
make -C "$ROOT_DIR" BOARD=qemu_virt
make -C "$ROOT_DIR" sdk
make -C "$SDK_EXAMPLE" clean all \
    SDK="$SDK_DIR" \
    EXTRA_CFLAGS=-DARMONIOS_EXTERNAL_KLI_SPAWN_TEST=1

if [[ ! -f "$SHELL_KLI" || ! -f "$HELLO_KLI" ]]; then
    echo 'FAIL: Shell or spawn-test HELLO.KLI artifact is missing' >&2
    exit 1
fi

"$HOST_CC" -std=c11 -Wall -Wextra -Werror -O2 \
    "$ROOT_DIR/tools/mkfat32_image.c" -o "$MKFAT"
"$MKFAT" "$FAT_IMAGE" "$SHELL_KLI" "$HELLO_KLI"

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
grep -q 'External spawn parent started' "$SERIAL_LOG" || {
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: external parent KLI did not start in EL0' >&2
    exit 1
}
grep -q 'Hello from an external ArmoniOS SDK app' "$SERIAL_LOG" || {
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: SYS_SPAWN child KLI did not execute from the VFS path' >&2
    exit 1
}
grep -q 'External spawn child exited cleanly' "$SERIAL_LOG" || {
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: external parent did not reap a clean child exit' >&2
    exit 1
}
grep -q 'USER exit: 0x0000000000000000' "$SERIAL_LOG" || {
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: external KLI process did not exit cleanly' >&2
    exit 1
}
grep -q 'panel:exit' "$SERIAL_LOG" || {
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: recovery wrapper did not observe the parent EL0 return' >&2
    exit 1
}
grep -q 'USER exit code: 0x0000000000000000' "$SERIAL_LOG" || {
    cat "$SERIAL_LOG" >&2
    echo 'FAIL: kernel did not regain control after external parent exit' >&2
    exit 1
}

printf 'PASS: external KLI spawned and reaped a VFS-loaded EL0 child\n'
printf 'QEMU log: %s\n' "$SERIAL_LOG"
