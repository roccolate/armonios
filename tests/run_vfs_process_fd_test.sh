#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/build/tests/vfs_process_fd"

mkdir -p "$(dirname "$OUT")"

"${CC:-gcc}" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR" \
    "$ROOT_DIR/tests/test_vfs_process_fd_standalone.c" \
    "$ROOT_DIR/kernel/vfs.c" \
    -o "$OUT"

"$OUT"
