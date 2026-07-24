#!/usr/bin/env bash

# Builds the generated SDK, copies it away from the repository build location,
# and recompiles the bundled example using only files inside that copied tree.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests/sdk-bundle"
SOURCE_SDK="$ROOT_DIR/build/sdk"
COPIED_SDK="$OUT_DIR/armonios-sdk"
HOST_CC="${HOST_CC:-cc}"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

make -C "$ROOT_DIR" sdk
cp -R "$SOURCE_SDK" "$COPIED_SDK"

if find "$COPIED_SDK" -type l -print -quit | grep -q .; then
    echo 'FAIL: copied SDK contains symbolic links' >&2
    exit 1
fi

if grep -R -nE '#include[[:space:]]*[<"](include/|kernel/)' \
        "$COPIED_SDK/include" >"$OUT_DIR/private-includes.log"; then
    cat "$OUT_DIR/private-includes.log" >&2
    echo 'FAIL: copied SDK exposes repository-private include roots' >&2
    exit 1
fi

make -C "$COPIED_SDK/examples/hello-console" clean all \
    SDK="$COPIED_SDK"

KLI="$COPIED_SDK/examples/hello-console/build/HELLO.KLI"
if [[ ! -f "$KLI" ]]; then
    echo 'FAIL: external SDK example did not produce HELLO.KLI' >&2
    exit 1
fi

cat > "$OUT_DIR/validate.c" <<'SRC'
#include <stdint.h>
#include <stdio.h>

#include <armonios/abi/kli.h>

static int fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(int argc, char **argv) {
    arm_kli1_header_t header;
    FILE *file;
    long size;

    if (argc != 2) {
        return fail("validator expects one KLI path");
    }
    file = fopen(argv[1], "rb");
    if (file == NULL) {
        return fail("cannot open HELLO.KLI");
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return fail("cannot seek HELLO.KLI");
    }
    size = ftell(file);
    if (size < 0 || size > 8192 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return fail("HELLO.KLI size is outside current loader limit");
    }
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return fail("cannot read KLI1 header");
    }
    fclose(file);

    if (header.magic != ARM_KLI1_MAGIC ||
        header.header_size != ARM_KLI1_HEADER_SIZE ||
        header.entry_count != 1U ||
        header.image_size != (uint64_t)size ||
        header.entry_offsets[0] < ARM_KLI1_HEADER_SIZE ||
        header.entry_offsets[0] >= header.image_size ||
        (header.entry_offsets[0] & 3U) != 0U) {
        return fail("generated SDK image violates KLI1 contract");
    }

    printf("PASS: isolated SDK produced valid %ld-byte HELLO.KLI\n", size);
    return 0;
}
SRC

"$HOST_CC" -std=c11 -Wall -Wextra -Werror \
    -I"$COPIED_SDK/include" "$OUT_DIR/validate.c" \
    -o "$OUT_DIR/validate"
"$OUT_DIR/validate" "$KLI"

printf 'PASS: copied SDK example builds without repository access\n'
