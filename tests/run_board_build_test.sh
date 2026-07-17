#!/usr/bin/env bash

# Verifies that `make BOARD=rpi4` compiles and links cleanly using a
# separate build directory so the qemu_virt artefact kept under the default
# `build/` is unaffected. RISK-006 (Raspberry Pi board contract is
# incomplete) required every function declared in drivers/board.h to
# resolve; this runner proves that milestone by deleting the rpi4 build
# directory, building with BOARD=rpi4, and checking the resulting kernel
# size gate.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BOARD_BUILD_DIR:-$ROOT_DIR/build-rpi4}"
KERNEL_BIN="$BUILD_DIR/kernel.bin"
KERNEL_SIZE_LIMIT="${KERNEL_SIZE_LIMIT:-108000}"

printf '\n=== BOARD=rpi4 (%s) ===\n' "$BUILD_DIR"
rm -rf "$BUILD_DIR"
make "BOARD=rpi4" "BUILD_DIR=$BUILD_DIR" -C "$ROOT_DIR"

if [[ ! -f "$KERNEL_BIN" ]]; then
    printf 'FAIL: BOARD=rpi4 build did not produce %s\n' "$KERNEL_BIN"
    exit 1
fi

size=$(stat -c%s "$KERNEL_BIN")
if (( size > KERNEL_SIZE_LIMIT )); then
    printf 'FAIL: BOARD=rpi4 kernel.bin is %d bytes, exceeds limit %d\n' \
        "$size" "$KERNEL_SIZE_LIMIT"
    exit 1
fi
printf 'PASS: BOARD=rpi4 kernel.bin size %d (limit %d)\n' \
    "$size" "$KERNEL_SIZE_LIMIT"
