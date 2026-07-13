#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests"
CC_BIN="${CC:-gcc}"

mkdir -p "$OUT_DIR"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR" \
    "$ROOT_DIR/tests/test_vfs_process_fd_standalone.c" \
    "$ROOT_DIR/kernel/vfs.c" \
    -o "$OUT_DIR/vfs_process_fd"
"$OUT_DIR/vfs_process_fd"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -DARMONIOS_TEST \
    -I"$ROOT_DIR" \
    "$ROOT_DIR/tests/test_process_fd_cleanup_standalone.c" \
    "$ROOT_DIR/kernel/process.c" \
    -o "$OUT_DIR/process_fd_cleanup"
"$OUT_DIR/process_fd_cleanup"
