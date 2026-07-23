#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC_BIN="${CC:-gcc}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

compile_and_run() {
    local source="$1"
    local output="$2"

    "$CC_BIN" \
        -std=c11 \
        -Wall -Wextra -Werror -pedantic \
        -I"$ROOT_DIR" \
        "$ROOT_DIR/tests/$source" \
        -o "$TMP_DIR/$output"

    "$TMP_DIR/$output"
}

compile_and_run libarmdesk_foundation_test.c libarmdesk_foundation_test
compile_and_run libarmdesk_widgets_test.c libarmdesk_widgets_test
printf 'PASS: libarmdesk foundation and widget models\n'
