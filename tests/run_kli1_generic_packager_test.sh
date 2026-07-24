#!/usr/bin/env bash

# Proves that the shared KLI1 packaging objects can build a valid application
# outside the source tree without per-application *_header.S / *_end.S files or
# private kernel headers.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests/kli1-generic-packager"
SDK_DIR="$OUT_DIR/sdk"
APP_DIR="$OUT_DIR/external-app"
CC="${CC:-aarch64-linux-gnu-gcc}"
LD="${LD:-aarch64-linux-gnu-ld}"
OBJCOPY="${OBJCOPY:-aarch64-linux-gnu-objcopy}"
HOST_CC="${HOST_CC:-cc}"

rm -rf "$OUT_DIR"
mkdir -p "$SDK_DIR/include/armonios/abi" "$SDK_DIR/lib" \
         "$SDK_DIR/linker" "$APP_DIR"

# Stage only the pieces a minimal future SDK needs for a flat KLI1 executable.
cp "$ROOT_DIR/include/armonios/abi/kli.h" \
   "$SDK_DIR/include/armonios/abi/kli.h"
cp "$ROOT_DIR/programs/libkarm/crt0.S" "$SDK_DIR/lib/crt0.S"
cp "$ROOT_DIR/programs/apps/kli1_header.S" "$SDK_DIR/linker/kli1_header.S"
cp "$ROOT_DIR/programs/apps/kli1_end.S" "$SDK_DIR/linker/kli1_end.S"
cp "$ROOT_DIR/programs/apps/image.ld" "$SDK_DIR/linker/image.ld"

cat > "$APP_DIR/main.c" <<'SRC'
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 0;
}
SRC

COMMON_FLAGS=(
    -Wall -Wextra -Werror
    -ffreestanding -nostdlib -nostartfiles
    -fno-builtin -fno-stack-protector -mgeneral-regs-only
    -fno-pic -fno-pie
    -ffunction-sections -fdata-sections
    -mcpu=cortex-a72 -Os -g
)

"$CC" "${COMMON_FLAGS[@]}" -std=c11 -I"$SDK_DIR/include" \
    -c "$APP_DIR/main.c" -o "$APP_DIR/main.o"
"$CC" "${COMMON_FLAGS[@]}" -c "$SDK_DIR/lib/crt0.S" \
    -o "$SDK_DIR/lib/crt0.o"
"$CC" "${COMMON_FLAGS[@]}" -c "$SDK_DIR/linker/kli1_header.S" \
    -o "$SDK_DIR/lib/kli1_header.o"
"$CC" "${COMMON_FLAGS[@]}" -c "$SDK_DIR/linker/kli1_end.S" \
    -o "$SDK_DIR/lib/kli1_end.o"

"$LD" --gc-sections -nostdlib -T "$SDK_DIR/linker/image.ld" \
    "$APP_DIR/main.o" \
    "$SDK_DIR/lib/kli1_header.o" \
    "$SDK_DIR/lib/crt0.o" \
    "$SDK_DIR/lib/kli1_end.o" \
    -o "$APP_DIR/hello.elf"
"$OBJCOPY" -O binary "$APP_DIR/hello.elf" "$APP_DIR/HELLO.KLI"

cat > "$OUT_DIR/validate.c" <<'SRC'
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <armonios/abi/kli.h>

static int fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main(int argc, char **argv) {
    arm_kli1_header_t header;
    FILE *file;
    long file_size;

    if (argc != 2) {
        return fail("validator expects one KLI path");
    }

    file = fopen(argv[1], "rb");
    if (file == NULL) {
        return fail("cannot open generated KLI");
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return fail("cannot seek generated KLI");
    }
    file_size = ftell(file);
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return fail("cannot measure generated KLI");
    }
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return fail("cannot read complete KLI1 header");
    }
    fclose(file);

    if (header.magic != ARM_KLI1_MAGIC) {
        return fail("wrong KLI1 magic");
    }
    if (header.header_size != ARM_KLI1_HEADER_SIZE ||
        header.entry_count != 1U) {
        return fail("wrong KLI1 header metadata");
    }
    if (header.image_size != (uint64_t)file_size) {
        return fail("image_size does not match generated file size");
    }
    if (header.entry_offsets[0] < ARM_KLI1_HEADER_SIZE ||
        header.entry_offsets[0] >= header.image_size ||
        (header.entry_offsets[0] & 3U) != 0U) {
        return fail("entry offset is outside or misaligned");
    }
    for (uint32_t i = 1; i < ARM_KLI1_MAX_ENTRIES; i++) {
        if (header.entry_offsets[i] != 0U) {
            return fail("unused KLI1 entry is non-zero");
        }
    }

    printf("PASS: generic KLI1 packager produced %ld-byte HELLO.KLI, entry=%llu\n",
           file_size, (unsigned long long)header.entry_offsets[0]);
    return 0;
}
SRC

"$HOST_CC" -std=c11 -Wall -Wextra -Werror -I"$SDK_DIR/include" \
    "$OUT_DIR/validate.c" -o "$OUT_DIR/validate"
"$OUT_DIR/validate" "$APP_DIR/HELLO.KLI"

printf 'PASS: external app built without repository-private headers\n'
