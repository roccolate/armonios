#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${FAT32_CORRUPTION_BUILD_DIR:-$ROOT_DIR/build-fat32-corruption}"
CC="${HOST_CC:-gcc}"
BINARY="$BUILD_DIR/fat32-corruption-test"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

"$CC" \
    -std=c11 -O0 -g -Wall -Wextra -Werror -DARMONIOS_TEST \
    -I"$ROOT_DIR" \
    "$ROOT_DIR/kernel/fat32.c" \
    "$ROOT_DIR/tests/test_fat32_corruption_standalone.c" \
    -o "$BINARY"

"$BINARY"
printf 'PASS: FAT32 geometry and corrupt-chain bounds\n'
