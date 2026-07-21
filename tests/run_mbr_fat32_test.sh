#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests"
CC_BIN="${CC:-gcc}"

mkdir -p "$OUT_DIR"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR" -I"$ROOT_DIR/drivers" \
    "$ROOT_DIR/tests/test_mbr_fat32_standalone.c" \
    "$ROOT_DIR/drivers/storage/mbr.c" \
    -o "$OUT_DIR/mbr-fat32"

"$OUT_DIR/mbr-fat32"
