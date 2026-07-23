#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests"
CC_BIN="${CC:-gcc}"

mkdir -p "$OUT_DIR"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR" -I"$ROOT_DIR/drivers" \
    "$ROOT_DIR/tests/test_block_view_fat32_standalone.c" \
    "$ROOT_DIR/drivers/storage/block_device.c" \
    "$ROOT_DIR/kernel/fat32.c" \
    "$ROOT_DIR/kernel/kstring.c" \
    -o "$OUT_DIR/block-device-view-fat32"

"$OUT_DIR/block-device-view-fat32"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR" -I"$ROOT_DIR/drivers" \
    "$ROOT_DIR/tests/test_storage_adapters_standalone.c" \
    -o "$OUT_DIR/storage-adapters"

"$OUT_DIR/storage-adapters"
