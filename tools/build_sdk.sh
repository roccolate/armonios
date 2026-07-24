#!/usr/bin/env bash

# Assemble the minimal external ArmoniOS SDK from public headers and built
# userland artifacts. This script never copies kernel-private headers or source.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build}"
SDK_DIR="${2:-$BUILD_DIR/sdk}"

case "$BUILD_DIR" in
    /*) ;;
    *) BUILD_DIR="$ROOT_DIR/$BUILD_DIR" ;;
esac
case "$SDK_DIR" in
    /*) ;;
    *) SDK_DIR="$ROOT_DIR/$SDK_DIR" ;;
esac

require_file() {
    if [[ ! -f "$1" ]]; then
        printf 'error: SDK input missing: %s\n' "$1" >&2
        exit 1
    fi
}

CRT0="$BUILD_DIR/programs/libkarm/crt0.o"
LIBKARM="$BUILD_DIR/programs/libkarm/libkarm.a"
KLI1_HEADER="$BUILD_DIR/programs/apps/kli1_header.o"
KLI1_END="$BUILD_DIR/programs/apps/kli1_end.o"

require_file "$CRT0"
require_file "$LIBKARM"
require_file "$KLI1_HEADER"
require_file "$KLI1_END"
require_file "$ROOT_DIR/programs/apps/image.ld"
require_file "$ROOT_DIR/tools/check_kli1_relocations.sh"
require_file "$ROOT_DIR/sdk/README.md"
require_file "$ROOT_DIR/sdk/examples/hello-console/main.c"
require_file "$ROOT_DIR/sdk/examples/hello-console/Makefile"

rm -rf "$SDK_DIR"
mkdir -p \
    "$SDK_DIR/include/armonios" \
    "$SDK_DIR/include/libkarm" \
    "$SDK_DIR/lib" \
    "$SDK_DIR/linker" \
    "$SDK_DIR/tools" \
    "$SDK_DIR/examples"

cp -R "$ROOT_DIR/include/armonios/abi" "$SDK_DIR/include/armonios/abi"
cp "$ROOT_DIR/programs/libkarm/"*.h "$SDK_DIR/include/libkarm/"
cp "$CRT0" "$SDK_DIR/lib/crt0.o"
cp "$LIBKARM" "$SDK_DIR/lib/libkarm.a"
cp "$KLI1_HEADER" "$SDK_DIR/lib/kli1_header.o"
cp "$KLI1_END" "$SDK_DIR/lib/kli1_end.o"
cp "$ROOT_DIR/programs/apps/image.ld" "$SDK_DIR/linker/image.ld"
cp "$ROOT_DIR/tools/check_kli1_relocations.sh" \
   "$SDK_DIR/tools/check_kli1_relocations.sh"
cp "$ROOT_DIR/sdk/README.md" "$SDK_DIR/README.md"
cp -R "$ROOT_DIR/sdk/examples/hello-console" \
      "$SDK_DIR/examples/hello-console"

if grep -R -nE '#include[[:space:]]*[<"](include/|kernel/)' \
        "$SDK_DIR/include" >"$SDK_DIR/private-includes.log"; then
    cat "$SDK_DIR/private-includes.log" >&2
    rm -f "$SDK_DIR/private-includes.log"
    echo 'error: SDK headers reference repository-private include roots' >&2
    exit 1
fi
rm -f "$SDK_DIR/private-includes.log"

if find "$SDK_DIR" -type l -print -quit | grep -q .; then
    echo 'error: SDK bundle must not contain symbolic links' >&2
    exit 1
fi

printf 'ArmoniOS SDK: %s\n' "$SDK_DIR"
