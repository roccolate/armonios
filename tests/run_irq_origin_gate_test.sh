#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/tests"
CC_BIN="${CC:-gcc}"
VECTOR_SOURCE="$ROOT_DIR/kernel/exception_vectors.S"
IRQ_ASM_SOURCE="$ROOT_DIR/kernel/irq_asm.S"

mkdir -p "$OUT_DIR"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$ROOT_DIR" \
    "$ROOT_DIR/tests/test_irq_origin_gate_standalone.c" \
    -o "$OUT_DIR/irq-origin-gate"

"$OUT_DIR/irq-origin-gate"

# Keep the architectural classifier and the vector wiring together. This gate
# intentionally checks the small assembly contract because host C tests cannot
# execute the AArch64 exception entry path.
grep -Fq "bl irq_handler_frame_from_vector" "$VECTOR_SOURCE"
grep -Fq ".global irq_handler_frame_from_vector" "$IRQ_ASM_SOURCE"
grep -Fq "ldr x1, [x0, #256]" "$IRQ_ASM_SOURCE"
grep -Fq "and x1, x1, #0x0f" "$IRQ_ASM_SOURCE"
grep -Fq "mov x0, xzr" "$IRQ_ASM_SOURCE"
grep -Fq "b irq_handler_frame" "$IRQ_ASM_SOURCE"

printf 'irq-origin-assembly-gate: PASS\n'
