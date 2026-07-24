#!/usr/bin/env bash

# Stages the public ABI and libkarm headers into an SDK-shaped include tree and
# compiles a consumer using only that tree. The normal build and host-test build
# must both expose the same installable include root validated here.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests/libkarm-sdk-headers"
SDK_INCLUDE="$OUT_DIR/sdk/include"
HOST_CC="${HOST_CC:-cc}"

rm -rf "$OUT_DIR"
mkdir -p "$SDK_INCLUDE/armonios" "$SDK_INCLUDE/libkarm"

cp -R "$ROOT_DIR/include/armonios/abi" "$SDK_INCLUDE/armonios/abi"
cp "$ROOT_DIR/programs/libkarm/"*.h "$SDK_INCLUDE/libkarm/"

if grep -R -nE '#include[[:space:]]*[<"](include/|kernel/)' \
        "$SDK_INCLUDE" >"$OUT_DIR/private-includes.log"; then
    cat "$OUT_DIR/private-includes.log" >&2
    echo "FAIL: staged SDK headers reference repository-private include roots" >&2
    exit 1
fi

cat > "$OUT_DIR/consumer.c" <<'SRC'
#include <armonios/abi/errors.h>
#include <armonios/abi/kli.h>
#include <armonios/abi/memory.h>
#include <armonios/abi/process.h>
#include <armonios/abi/syscall_numbers.h>
#include <armonios/abi/system.h>
#include <armonios/abi/vfs.h>

#include <libkarm/arena.h>
#include <libkarm/buffer.h>
#include <libkarm/dynamic_string.h>
#include <libkarm/errno.h>
#include <libkarm/file.h>
#include <libkarm/string.h>
#include <libkarm/syscall.h>

static int consume_headers(void) {
    kli_arena_t arena = {0};
    kli_buffer_t buffer = {0};
    kli_string_t string = {0};
    arm_kli1_header_t image = {0};
    arm_stat_v2_t stat = {0};

    image.magic = ARM_KLI1_MAGIC;
    stat.version = ARM_VFS_METADATA_VERSION;
    stat.struct_size = sizeof(stat);

    return (int)(arena.offset + buffer.length + string.buffer.length +
                 image.magic + stat.version);
}

int main(void) {
    return consume_headers() == 0;
}
SRC

"$HOST_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$SDK_INCLUDE" -fsyntax-only "$OUT_DIR/consumer.c"

printf 'PASS: libkarm headers compile from isolated SDK include tree\n'
