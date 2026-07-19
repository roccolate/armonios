#!/usr/bin/env bash

# Verifies the KLI1 mutable-storage contract on the shipping apps.
#
# The .user_image layout (programs/apps/image.ld) forbids .data and .bss in
# the flat image. Any app needing mutable static state must obtain it through
# SYS_MMAP at runtime.
#
# This runner checks three things per app:
#   1. The shippable .elf links cleanly under the production user image.ld.
#   2. The resulting ELF has no .data or .bss sections.
#   3. A regression build that *would* emit .bss is rejected by the linker
#      ASSERT, so the contract is genuinely enforced rather than silently
#      dropping the offending section.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests"
BIN_DIR="$ROOT_DIR/build/programs/apps"
OBJDUMP="${OBJDUMP:-aarch64-linux-gnu-objdump}"

mkdir -p "$OUT_DIR"

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
