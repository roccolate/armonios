#include "kernel/panel_boot_argv.h"

#include <stdint.h>

static void store_u64(uint8_t *dst, uint64_t value) {
    for (uint32_t i = 0; i < sizeof(uint64_t); i++) {
        dst[i] = (uint8_t)(value >> (i * 8U));
    }
}

int panel_boot_place_argv_on_stack(uint8_t *stack, uint64_t stack_base,
                                   uint64_t stack_size,
                                   const panel_boot_argv_t *argv,
                                   uint32_t argc,
                                   uint64_t *out_argv_vaddr) {
    uint64_t pointer_bytes;
    uint64_t total;
    uint64_t argv_vaddr;
    uint64_t strings_vaddr;
    uint64_t strings_offset;

    if (stack == 0 || out_argv_vaddr == 0 || stack_size == 0U) {
        return -1;
    }
    if (argc == 0U) {
        *out_argv_vaddr = 0U;
        return 0;
    }
    if (argv == 0 || argc > PANEL_BOOT_ARGV_MAX_STRINGS ||
        argv->bytes_used == 0U ||
        argv->bytes_used > PANEL_BOOT_ARGV_MAX_BYTES) {
        return -1;
    }
    for (uint32_t i = 0; i < argc; i++) {
        if (argv->offsets[i] >= argv->bytes_used) {
            return -1;
        }
    }

    pointer_bytes = (uint64_t)(argc + 1U) * sizeof(uint64_t);
    total = (pointer_bytes + argv->bytes_used + 15U) & ~(uint64_t)15U;
    if (total > stack_size || stack_base > UINT64_MAX - stack_size) {
        return -1;
    }

    argv_vaddr = stack_base + stack_size - total;
    strings_vaddr = argv_vaddr + pointer_bytes;
    strings_offset = strings_vaddr - stack_base;

    for (uint32_t i = 0; i < argv->bytes_used; i++) {
        stack[strings_offset + i] = (uint8_t)argv->bytes[i];
    }
    for (uint32_t i = 0; i < argc; i++) {
        store_u64(stack + (argv_vaddr - stack_base) +
                      (uint64_t)i * sizeof(uint64_t),
                  strings_vaddr + argv->offsets[i]);
    }
    store_u64(stack + (argv_vaddr - stack_base) +
                  (uint64_t)argc * sizeof(uint64_t),
              0U);

    *out_argv_vaddr = argv_vaddr;
    return 0;
}
