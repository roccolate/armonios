#ifndef ARMONIOS_KERNEL_MM_MMU_H
#define ARMONIOS_KERNEL_MM_MMU_H

#include <stdint.h>

/**
 * mmu_enable_identity - Enable EL1 translation using an identity-mapped PGD.
 *
 * Caches remain disabled in this early bring-up step; only SCTLR_EL1.M is set.
 */
void mmu_enable_identity(uint64_t *pgd);

/**
 * mmu_set_ttbr0 - Install the lower-address-space page table root.
 */
void mmu_set_ttbr0(uint64_t *pgd);

/**
 * mmu_read_ttbr0_el1 - Read the active TTBR0_EL1 value.
 */
uint64_t mmu_read_ttbr0_el1(void);

/**
 * mmu_read_sctlr_el1 - Read SCTLR_EL1 for smoke-test diagnostics.
 */
uint64_t mmu_read_sctlr_el1(void);

#endif
