#ifndef ARMONIOS_KERNEL_AARCH64_STATE_H
#define ARMONIOS_KERNEL_AARCH64_STATE_H

#include <stdint.h>

/*
 * Saved Program Status Register values used when returning through
 * the EL0/EL1 exception frame.
 *
 * Keep these architectural constants out of generic layout headers: they
 * describe CPU exception-return state, not memory placement.
 */
#define AARCH64_SPSR_MODE_MASK 0x0fULL
#define AARCH64_SPSR_MODE_EL0T 0x00ULL
#define AARCH64_SPSR_MODE_EL1H 0x05ULL

#define AARCH64_SPSR_EL0T_DAF_MASKED  0x340ULL
#define AARCH64_SPSR_EL1H_DAIF_MASKED 0x3c5ULL

static inline int aarch64_spsr_returns_to_el0(uint64_t spsr) {
    return (spsr & AARCH64_SPSR_MODE_MASK) == AARCH64_SPSR_MODE_EL0T;
}

#endif
