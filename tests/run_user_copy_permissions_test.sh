#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/build/tests/user_copy_permissions"

mkdir -p "$(dirname "$OUT")"

"${CC:-gcc}" -std=c11 -Wall -Wextra -Werror \
    -D_POSIX_C_SOURCE=200112L \
    -I"$ROOT_DIR" \
    "$ROOT_DIR/tests/test_user_copy_permissions_standalone.c" \
    "$ROOT_DIR/kernel/syscall_helpers.c" \
    -o "$OUT"

"$OUT"
