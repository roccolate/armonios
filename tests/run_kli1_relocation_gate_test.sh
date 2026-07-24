#!/usr/bin/env bash

# Proves that the KLI1 relocation checker accepts the normal page-relative
# AArch64 code model and rejects a read-only table containing an absolute
# function pointer that would be invalid after loading the image away from zero.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests/kli1-relocations"
CC="${CC:-aarch64-linux-gnu-gcc}"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

cat > "$OUT_DIR/safe.c" <<'SRC'
static int plus_one(int value) {
    return value + 1;
}

int main(int argc, char **argv) {
    int (*callback)(int);

    (void)argv;
    callback = plus_one;
    return callback(argc) == argc + 1 ? 0 : 1;
}
SRC

cat > "$OUT_DIR/absolute_pointer.c" <<'SRC'
typedef int (*callback_t)(int);

static int plus_one(int value) {
    return value + 1;
}

__attribute__((used, section(".rodata.kli1_absolute_pointer")))
static callback_t const callbacks[] = {
    plus_one,
};

int main(int argc, char **argv) {
    (void)argv;
    return callbacks[0](argc);
}
SRC

COMMON_FLAGS=(
    -Wall -Wextra -Werror
    -ffreestanding -nostdlib -nostartfiles
    -fno-builtin -fno-stack-protector -mgeneral-regs-only
    -fno-pic -fno-pie
    -ffunction-sections -fdata-sections
    -mcpu=cortex-a72 -O0
)

"$CC" "${COMMON_FLAGS[@]}" -std=c11 -c "$OUT_DIR/safe.c" \
    -o "$OUT_DIR/safe.o"
"$CC" "${COMMON_FLAGS[@]}" -std=c11 -c "$OUT_DIR/absolute_pointer.c" \
    -o "$OUT_DIR/absolute_pointer.o"
"$CC" "${COMMON_FLAGS[@]}" -c "$ROOT_DIR/programs/libkarm/crt0.S" \
    -o "$OUT_DIR/crt0.o"
"$CC" "${COMMON_FLAGS[@]}" -c "$ROOT_DIR/programs/apps/kli1_header.S" \
    -o "$OUT_DIR/kli1_header.o"
"$CC" "${COMMON_FLAGS[@]}" -c "$ROOT_DIR/programs/apps/kli1_end.S" \
    -o "$OUT_DIR/kli1_end.o"

bash "$ROOT_DIR/tools/check_kli1_relocations.sh" \
    "$OUT_DIR/safe.o" \
    "$OUT_DIR/crt0.o" \
    "$OUT_DIR/kli1_header.o" \
    "$OUT_DIR/kli1_end.o"

if bash "$ROOT_DIR/tools/check_kli1_relocations.sh" \
        "$OUT_DIR/absolute_pointer.o" \
        >"$OUT_DIR/negative.log" 2>&1; then
    echo "FAIL: absolute function-pointer table passed KLI1 relocation gate" >&2
    exit 1
fi

if ! grep -q 'R_AARCH64_ABS64' "$OUT_DIR/negative.log"; then
    cat "$OUT_DIR/negative.log" >&2
    echo "FAIL: negative case failed without identifying R_AARCH64_ABS64" >&2
    exit 1
fi

printf 'PASS: KLI1 relocation gate rejected absolute function-pointer table\n'
