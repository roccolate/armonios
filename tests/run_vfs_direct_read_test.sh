#!/usr/bin/env bash

# This standalone gate links the real VFS dispatcher with a mount backend that
# records `open` calls, proving direct path reads do not borrow an EL0 FD.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests"
CC_BIN="${CC:-gcc}"
BINARY="$OUT_DIR/vfs_direct_read"

mkdir -p "$OUT_DIR"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR" -I"$ROOT_DIR/drivers" \
    "$ROOT_DIR/tests/test_vfs_direct_read.c" \
    "$ROOT_DIR/kernel/vfs.c" \
    "$ROOT_DIR/kernel/kstring.c" \
    -o "$BINARY"

"$BINARY"
printf 'PASS: direct VFS path reads bypass process file descriptors\n'
