#!/usr/bin/env bash

# Verifies both Raspberry Pi build contracts:
#
# 1. the normal BOARD=rpi4 image remains serial/bootfs fail-closed;
# 2. the explicitly requested EMMC2 probe path compiles and produces a
#    kernel8.img without changing the normal board capabilities.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BOARD_BUILD_DIR:-$ROOT_DIR/build-rpi4}"
PROBE_BUILD_DIR="${BOARD_PROBE_BUILD_DIR:-$ROOT_DIR/build-rpi4-emmc2-probe}"
KERNEL_SIZE_LIMIT="${KERNEL_SIZE_LIMIT:-131072}"

check_image() {
    local name="$1"
    local image="$2"
    local size

    if [[ ! -f "$image" ]]; then
        printf 'FAIL: %s build did not produce %s\n' "$name" "$image"
        exit 1
    fi

    size=$(stat -c%s "$image")
    if (( size > KERNEL_SIZE_LIMIT )); then
        printf 'FAIL: %s image is %d bytes, exceeds limit %d\n' \
            "$name" "$size" "$KERNEL_SIZE_LIMIT"
        exit 1
    fi
    printf 'PASS: %s image size %d (limit %d)\n' \
        "$name" "$size" "$KERNEL_SIZE_LIMIT"
}

printf '\n=== BOARD=rpi4 (%s) ===\n' "$BUILD_DIR"
rm -rf "$BUILD_DIR"
make "BOARD=rpi4" "BUILD_DIR=$BUILD_DIR" -C "$ROOT_DIR"
check_image "BOARD=rpi4" "$BUILD_DIR/kernel.bin"

printf '\n=== RPi4 EMMC2 probe (%s) ===\n' "$PROBE_BUILD_DIR"
rm -rf "$PROBE_BUILD_DIR"
make -C "$ROOT_DIR" \
    "RPI4_PROBE_BUILD_DIR=$PROBE_BUILD_DIR" rpi4-emmc2-probe
check_image "RPi4 EMMC2 probe" "$PROBE_BUILD_DIR/kernel8.img"
