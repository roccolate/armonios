#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT_DIR/build/tests/panel_model"

mkdir -p "$(dirname "$OUT")"

"${CC:-gcc}" -std=c11 -Wall -Wextra -Werror -I"$ROOT_DIR" \
    "$ROOT_DIR/tests/test_panel_model_standalone.c" -o "$OUT"

"$OUT"
