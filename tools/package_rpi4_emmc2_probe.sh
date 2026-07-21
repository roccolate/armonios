#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${RPI4_PROBE_BUILD_DIR:-$ROOT_DIR/build-rpi4-emmc2-probe}"
PACKAGE_DIR="${RPI4_PROBE_PACKAGE_DIR:-$BUILD_DIR/package}"
KERNEL_IMAGE="$BUILD_DIR/kernel8.img"
DEPLOY_DIR="$ROOT_DIR/deploy/rpi4-emmc2-probe"
KERNEL_SIZE_LIMIT="${KERNEL_SIZE_LIMIT:-108000}"

if [[ ! -f "$KERNEL_IMAGE" ]]; then
    make -C "$ROOT_DIR" rpi4-emmc2-probe
fi

for source in "$KERNEL_IMAGE" "$DEPLOY_DIR/config.txt" "$DEPLOY_DIR/README.md"; do
    if [[ ! -f "$source" ]]; then
        printf 'FAIL: required probe file is missing: %s\n' "$source" >&2
        exit 1
    fi
done

kernel_size=$(stat -c%s "$KERNEL_IMAGE")
if (( kernel_size > KERNEL_SIZE_LIMIT )); then
    printf 'FAIL: RPi4 probe image is %d bytes, exceeds limit %d\n' \
        "$kernel_size" "$KERNEL_SIZE_LIMIT" >&2
    exit 1
fi
printf 'PASS: RPi4 probe image size %d (limit %d)\n' \
    "$kernel_size" "$KERNEL_SIZE_LIMIT"

rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"
cp "$KERNEL_IMAGE" "$PACKAGE_DIR/kernel8.img"
cp "$DEPLOY_DIR/config.txt" "$PACKAGE_DIR/config.txt"
cp "$DEPLOY_DIR/README.md" "$PACKAGE_DIR/README.md"

git -C "$ROOT_DIR" rev-parse HEAD > "$PACKAGE_DIR/COMMIT"
(
    cd "$PACKAGE_DIR"
    sha256sum kernel8.img config.txt README.md COMMIT > SHA256SUMS
    sha256sum -c SHA256SUMS
)

printf 'RPi4 EMMC2 probe package: %s\n' "$PACKAGE_DIR"
printf 'commit: %s\n' "$(cat "$PACKAGE_DIR/COMMIT")"
printf 'kernel sha256: %s\n' "$(awk '$2 == "kernel8.img" { print $1 }' "$PACKAGE_DIR/SHA256SUMS")"
