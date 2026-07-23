#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests"
CC_BIN="${CC:-gcc}"
BINARY="$OUT_DIR/fat32_directory_traversal"

mkdir -p "$OUT_DIR"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -DARMONIOS_FAT32_DIRECTORY_STANDALONE \
    -I"$ROOT_DIR" -I"$ROOT_DIR/drivers" \
    "$ROOT_DIR/tests/test_fat32_directories.c" \
    "$ROOT_DIR/kernel/fat32.c" \
    "$ROOT_DIR/kernel/fat32_directory.c" \
    "$ROOT_DIR/kernel/fat32_vfs.c" \
    "$ROOT_DIR/kernel/vfs.c" \
    "$ROOT_DIR/kernel/kstring.c" \
    -o "$BINARY"

"$BINARY"
printf 'PASS: nested FAT32 directory traversal\n'
