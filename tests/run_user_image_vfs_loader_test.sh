#!/usr/bin/env bash

# This host gate links the real KLI1 parser with a controlled VFS backend and
# covers fragmented progress, malformed files, and the fixed image-slot bound.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests"
CC_BIN="${CC:-gcc}"
BINARY="$OUT_DIR/user_image_vfs_loader"

mkdir -p "$OUT_DIR"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR" -I"$ROOT_DIR/drivers" \
    "$ROOT_DIR/tests/test_user_image_vfs_loader.c" \
    "$ROOT_DIR/kernel/user_image.c" \
    -o "$BINARY"

"$BINARY"
printf 'PASS: KLI1 loader reads and validates complete VFS files\n'
