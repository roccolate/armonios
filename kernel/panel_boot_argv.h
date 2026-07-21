#ifndef ARMONIOS_KERNEL_PANEL_BOOT_ARGV_H
#define ARMONIOS_KERNEL_PANEL_BOOT_ARGV_H

#include <stdint.h>

#define PANEL_BOOT_ARGV_MAX_STRINGS 8U
#define PANEL_BOOT_ARGV_MAX_BYTES   256U

typedef struct {
    uint16_t offsets[PANEL_BOOT_ARGV_MAX_STRINGS];
    uint16_t bytes_used;
    char bytes[PANEL_BOOT_ARGV_MAX_BYTES];
} panel_boot_argv_t;

/*
 * Pack a kernel-owned argv byte block into a new EL0 stack. Each offset names
 * one NUL-terminated string inside `bytes`; no source pointers cross into the
 * loader.
 */
int panel_boot_place_argv_on_stack(uint8_t *stack, uint64_t stack_base,
                                   uint64_t stack_size,
                                   const panel_boot_argv_t *argv,
                                   uint32_t argc,
                                   uint64_t *out_argv_vaddr);

#endif
