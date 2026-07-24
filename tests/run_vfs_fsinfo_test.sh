#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests"
CC_BIN="${CC:-gcc}"
NATIVE_BINARY="$OUT_DIR/vfs_fsinfo"
SYSCALL_BINARY="$OUT_DIR/syscall_vfs_fsinfo"

mkdir -p "$OUT_DIR"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$ROOT_DIR" -I"$ROOT_DIR/drivers" \
    "$ROOT_DIR/tests/test_vfs_fsinfo.c" \
    "$ROOT_DIR/kernel/vfs.c" \
    "$ROOT_DIR/kernel/vfs_fsinfo.c" \
    "$ROOT_DIR/kernel/kstring.c" \
    -o "$NATIVE_BINARY"

"$NATIVE_BINARY"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$ROOT_DIR" -I"$ROOT_DIR/drivers" \
    "$ROOT_DIR/tests/test_syscall_vfs_fsinfo.c" \
    "$ROOT_DIR/kernel/syscall_vfs_fsinfo.c" \
    -o "$SYSCALL_BINARY"

"$SYSCALL_BINARY"
printf 'PASS: filesystem errors and fsinfo ABI\n'
