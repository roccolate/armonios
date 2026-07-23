#include <stdint.h>
#include <stdio.h>

#include "kernel/aarch64_state.h"

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "assert failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

int main(void) {
    ASSERT_TRUE(aarch64_spsr_returns_to_el0(AARCH64_SPSR_MODE_EL0T));
    ASSERT_TRUE(aarch64_spsr_returns_to_el0(
        AARCH64_SPSR_EL0T_DAF_MASKED));

    ASSERT_TRUE(!aarch64_spsr_returns_to_el0(AARCH64_SPSR_MODE_EL1H));
    ASSERT_TRUE(!aarch64_spsr_returns_to_el0(
        AARCH64_SPSR_EL1H_DAIF_MASKED));

    /* The reproduced VMM panic was taken from EL1h with IRQ unmasked. */
    ASSERT_TRUE(!aarch64_spsr_returns_to_el0(0x345ULL));

    puts("irq-origin-gate-test: PASS");
    return 0;
}
