#!/usr/bin/env bash

# Exercise the standalone canonical source selector before the process lifecycle
# consumes it, keeping bootfs namespace and VFS basename rules independently
# testable.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests"
CC_BIN="${CC:-gcc}"
BINARY="$OUT_DIR/app_image_source"

mkdir -p "$OUT_DIR"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR" -I"$ROOT_DIR/drivers" \
    "$ROOT_DIR/tests/test_app_image_source.c" \
    "$ROOT_DIR/kernel/app_image_source.c" \
    "$ROOT_DIR/kernel/vfs.c" \
    "$ROOT_DIR/kernel/kstring.c" \
    -o "$BINARY"

"$BINARY"
printf 'PASS: application paths resolve to bootfs or canonical VFS sources\n'
