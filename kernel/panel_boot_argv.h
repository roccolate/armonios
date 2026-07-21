#ifndef ARMONIOS_KERNEL_PANEL_BOOT_ARGV_H
#define ARMONIOS_KERNEL_PANEL_BOOT_ARGV_H

#include <stdint.h>

#define PANEL_BOOT_ARGV_MAX_STRINGS 8U
#define PANEL_BOOT_ARGV_MAX_BYTES   256U

/*
 * Pack kernel-owned argv strings and the argv pointer array into a new EL0
 * stack. Syscall callers must copy the pointer array and every string across
 * the EL0 boundary before invoking the app loader.
 *
 * `stack` is the kernel-accessible backing memory for the user stack, mapped
 * at `stack_base` for the process. The returned argv vaddr is also the initial
 * SP for argc > 0 and is always 16-byte aligned for AArch64.
 */
int panel_boot_place_argv_on_stack(uint8_t *stack, uint64_t stack_base,
                                   uint64_t stack_size,
                                   const uint64_t *argv_ptr,
                                   uint32_t argc,
                                   uint64_t *out_argv_vaddr);

#endif
