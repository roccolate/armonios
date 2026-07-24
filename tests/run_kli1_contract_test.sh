#!/usr/bin/env bash

# Verifies the public KLI1 ABI and mutable-storage contract.
#
# include/armonios/abi/kli.h is the SDK-facing source of truth for the on-disk
# header. The .user_image layout (programs/apps/image.ld) forbids .data and .bss
# in the flat image. Any app needing mutable static state must obtain it through
# SYS_MMAP at runtime.
#
# This runner checks:
#   1. the public header compiles standalone with only -I include;
#   2. its constants, offsets, and total size remain the KLI1 wire layout;
#   3. every shipping .elf links under the production user image.ld;
#   4. every shipping ELF has no .data or .bss sections;
#   5. a regression source that emits .bss is rejected by the linker ASSERT.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests"
BIN_DIR="$ROOT_DIR/build/programs/apps"
HOST_CC="${HOST_CC:-cc}"
OBJDUMP="${OBJDUMP:-aarch64-linux-gnu-objdump}"

mkdir -p "$OUT_DIR"

check_public_abi() {
    local src="$OUT_DIR/kli1_public_abi.c"
    local bin="$OUT_DIR/kli1_public_abi"

    cat > "$src" <<'SRC'
#include <stddef.h>
#include <stdint.h>

#include <armonios/abi/kli.h>

_Static_assert(ARM_KLI1_MAGIC == 0x31494c4bU, "KLI1 magic");
_Static_assert(ARM_KLI1_HEADER_SIZE == 80U, "KLI1 header size");
_Static_assert(ARM_KLI1_MAX_ENTRIES == 8U, "KLI1 entry capacity");
_Static_assert(sizeof(arm_kli1_header_t) == 80U, "KLI1 wire size");
_Static_assert(offsetof(arm_kli1_header_t, magic) == 0U, "KLI1 magic offset");
_Static_assert(offsetof(arm_kli1_header_t, header_size) == 4U,
               "KLI1 header_size offset");
_Static_assert(offsetof(arm_kli1_header_t, entry_count) == 6U,
               "KLI1 entry_count offset");
_Static_assert(offsetof(arm_kli1_header_t, image_size) == 8U,
               "KLI1 image_size offset");
_Static_assert(offsetof(arm_kli1_header_t, entry_offsets) == 16U,
               "KLI1 entry_offsets offset");

int main(void) {
    arm_kli1_header_t header = {0};

    header.magic = ARM_KLI1_MAGIC;
    header.header_size = ARM_KLI1_HEADER_SIZE;
    header.entry_count = 1U;
    header.image_size = ARM_KLI1_HEADER_SIZE + 4U;
    header.entry_offsets[0] = ARM_KLI1_HEADER_SIZE;

    return header.magic == 0x31494c4bU &&
                   header.entry_offsets[0] < header.image_size
               ? 0
               : 1;
}
SRC

    "$HOST_CC" -std=c11 -Wall -Wextra -Werror \
        -I"$ROOT_DIR/include" "$src" -o "$bin"
    "$bin"
}

check_elf_clean() {
    local elf="$1"
    local app="$2"

    if [[ ! -f "$elf" ]]; then
        printf 'FAIL: %s ELF not found at %s\n' "$app" "$elf"
        return 1
    fi

    if "$OBJDUMP" -h "$elf" | awk '/^ *[0-9]+ \./ {print $2}' | grep -Eq '^(\.data|\.bss)$'; then
        printf 'FAIL: %s ELF must not carry .data or .bss: %s\n' "$app" "$elf"
        "$OBJDUMP" -h "$elf" | grep -E '\.data|\.bss' || true
        return 1
    fi
}

assert_negative_case() {
    local app="$1"
    local src="$2"
    local cc_bin="${CC:-aarch64-linux-gnu-gcc}"

    if "$cc_bin" -std=c11 -ffreestanding -nostdlib -nostartfiles \
        -mcpu=cortex-a72 -Wall -Wextra -Werror \
        -I"$ROOT_DIR/programs" -I"$ROOT_DIR/programs/libkarm" \
        "$src" "$ROOT_DIR/programs/libkarm/syscall.S" \
        "$ROOT_DIR/programs/apps/${app}_header.S" \
        "$ROOT_DIR/programs/apps/${app}_end.S" \
        -T "$ROOT_DIR/programs/apps/image.ld" \
        -o "$OUT_DIR/kli1_violation.elf" >/dev/null 2>"$OUT_DIR/kli1_violation.log"; then
        printf 'FAIL: %s bss violation linked cleanly: contract not enforced\n' "$app"
        cat "$OUT_DIR/kli1_violation.log" || true
        return 1
    fi

    if ! grep -Fq "KLI1 forbids .bss" "$OUT_DIR/kli1_violation.log"; then
        printf 'FAIL: %s bss violation failed but not for KLI1 reason\n' "$app"
        cat "$OUT_DIR/kli1_violation.log" || true
        return 1
    fi
}

passed=0
failed=0

if check_public_abi; then
    printf 'PASS: public KLI1 header is standalone and layout-stable\n'
    passed=$((passed + 1))
else
    printf 'FAIL: public KLI1 header contract\n'
    failed=$((failed + 1))
fi

apps=(clock editor files monitor shell panel control)

for app in "${apps[@]}"; do
    if check_elf_clean "$BIN_DIR/$app.elf" "$app"; then
        printf 'PASS: %s ELF has no .data or .bss sections\n' "$app"
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
done

# Regression: confirm a fake app with a .bss symbol is rejected by the linker
# ASSERT in image.ld. The symbol is intentionally module-local so it does not
# collide with anything else that might be linked.
REGR_SRC="$OUT_DIR/kli1_bss_regression.c"
cat > "$REGR_SRC" <<'SRC'
#include <stdint.h>
static int dummy_storage[4];
int main(int argc, char **argv) {
    (void)argv;
    dummy_storage[0] = argc;
    return dummy_storage[0];
}
SRC

for app in "${apps[@]}"; do
    if assert_negative_case "$app" "$REGR_SRC"; then
        printf 'PASS: %s bss-regression rejected by KLI1 contract\n' "$app"
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
done

if (( failed > 0 )); then
    printf 'FAIL: %d checks failed\n' "$failed"
    exit 1
fi

printf 'PASS: KLI1 contract holds on %d/%d checks\n' "$passed" "$passed"
