#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC_BIN="${CC:-gcc}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

"$CC_BIN" \
    -std=c11 \
    -Wall -Wextra -Werror -pedantic \
    -I"$ROOT_DIR" \
    "$ROOT_DIR/tests/libarmdesk_foundation_test.c" \
    -o "$TMP_DIR/libarmdesk_foundation_test"

"$TMP_DIR/libarmdesk_foundation_test"
printf 'PASS: libarmdesk foundation\n'
