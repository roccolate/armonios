/*
 * Shared syscall-boundary validation.
 *
 * Syscall bodies should not re-open-code user pointer checks or owner-window
 * checks. Keeping those rules here preserves the exact error-code contract
 * while giving host tests one small surface to exercise.
 */

#include "kernel/syscall_helpers.h"

#include <stdint.h>

#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"

/* AArch64 level-3 leaf descriptor access bits used by vmm.c. */
#define USER_PTE_VALID   (1ULL << 0)
#define USER_PTE_PAGE    (1ULL << 1)
#define USER_PTE_AP_USER (1ULL << 6)
#define USER_PTE_AP_RO   (1ULL << 7)

static int64_t owner_window_lookup(process_t *process, uint64_t window_id,
                                   int64_t missing_error,
                                   gui_desktop_t **out_desktop,
                                   gui_window_t **out_window) {
    gui_desktop_t *desktop;
    gui_window_t *window;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS || out_window == 0) {
        return ERR_INVAL;
    }

    desktop = gui_desktop();
    if (desktop == 0) {
        return ERR_AGAIN;
    }

    window = &desktop->windows[window_id];
    if (window->used == 0) {
        return missing_error;
    }
    if (window->owner_pid != process->pid) {
        return ERR_BADF;
    }

    if (out_desktop != 0) {
        *out_desktop = desktop;
    }
    *out_window = window;
    return 0;
}

int64_t sys_owner_window(process_t *process, uint64_t window_id,
                         gui_desktop_t **out_desktop,
                         gui_window_t **out_window) {
    return owner_window_lookup(process, window_id, ERR_NOENT, out_desktop,
                               out_window);
}

int64_t sys_owner_window_badf(process_t *process, uint64_t window_id,
                              gui_desktop_t **out_desktop,
                              gui_window_t **out_window) {
    return owner_window_lookup(process, window_id, ERR_BADF, out_desktop,
                               out_window);
}

static int64_t user_buf_range(const process_t *process, uint64_t ptr,
                              uint64_t len, int require_write) {
    uint64_t last;
    uint64_t page;
    uint64_t last_page;

    if (process == 0 || (ptr == 0 && len != 0)) {
        return ERR_INVAL;
    }
    if (len == 0) {
        return 0;
    }

    if (!process_user_range_contains(process, ptr, len) ||
        process->page_table == 0 || ptr > UINT64_MAX - (len - 1ULL)) {
        return ERR_INVAL;
    }

    last = ptr + len - 1ULL;
    page = ptr & ~(PAGE_SIZE - 1ULL);
    last_page = last & ~(PAGE_SIZE - 1ULL);

    for (;;) {
        uint64_t entry = vmm_leaf_entry(process->page_table, page);

        if ((entry & (USER_PTE_VALID | USER_PTE_PAGE)) !=
                (USER_PTE_VALID | USER_PTE_PAGE) ||
            (entry & USER_PTE_AP_USER) == 0) {
            return ERR_INVAL;
        }
        if (require_write != 0 && (entry & USER_PTE_AP_RO) != 0) {
            return ERR_PERM;
        }
        if (page == last_page) {
            break;
        }
        if (page > UINT64_MAX - PAGE_SIZE) {
            return ERR_INVAL;
        }
        page += PAGE_SIZE;
    }

    return 0;
}

int64_t sys_user_buf_in(const process_t *process, uint64_t ptr, uint64_t len) {
    return user_buf_range(process, ptr, len, 0);
}

int64_t sys_user_buf_out(const process_t *process, uint64_t ptr, uint64_t len) {
    return user_buf_range(process, ptr, len, 1);
}

int64_t sys_copy_from_user(const process_t *process, void *out, uint64_t ptr,
                           uint64_t len) {
    uint8_t *dst = (uint8_t *)out;
    const uint8_t *src = (const uint8_t *)(uintptr_t)ptr;
    int64_t status;

    if (out == 0 && len != 0) {
        return ERR_INVAL;
    }

    status = sys_user_buf_in(process, ptr, len);
    if (status != 0) {
        return status;
    }

    for (uint64_t i = 0; i < len; i++) {
        dst[i] = src[i];
    }
    return 0;
}

void sys_copy_to_user_validated(uint64_t ptr, const void *input, uint64_t len) {
    uint8_t *dst = (uint8_t *)(uintptr_t)ptr;
    const uint8_t *src = (const uint8_t *)input;

    for (uint64_t i = 0; i < len; i++) {
        dst[i] = src[i];
    }
}

int64_t sys_copy_to_user(const process_t *process, uint64_t ptr,
                         const void *input, uint64_t len) {
    int64_t status;

    if (input == 0 && len != 0) {
        return ERR_INVAL;
    }

    status = sys_user_buf_out(process, ptr, len);
    if (status != 0) {
        return status;
    }

    sys_copy_to_user_validated(ptr, input, len);
    return 0;
}

int64_t sys_user_copy_cstr(const process_t *process, uint64_t ptr,
                           char *out, uint64_t capacity) {
    if (process == 0 || ptr == 0 || out == 0 || capacity == 0) {
        return ERR_INVAL;
    }

    for (uint64_t i = 0; i < capacity; i++) {
        uint64_t byte_ptr;

        if (ptr > UINT64_MAX - i) {
            return ERR_INVAL;
        }
        byte_ptr = ptr + i;

        if (sys_copy_from_user(process, &out[i], byte_ptr, 1) != 0) {
            return ERR_INVAL;
        }
        if (out[i] == '\0') {
            return 0;
        }
    }

    return ERR_INVAL;
}

int64_t sys_copy_argv_from_user(const process_t *process, uint64_t argv_ptr,
                                uint32_t argc,
                                panel_boot_argv_t *kernel_argv) {
    uint32_t used = 0U;

    if (process == 0 || kernel_argv == 0 ||
        argc > PANEL_BOOT_ARGV_MAX_STRINGS ||
        (argc == 0U && argv_ptr != 0U) ||
        (argc != 0U && argv_ptr == 0U)) {
        return ERR_INVAL;
    }

    for (uint32_t i = 0; i < argc; i++) {
        uint64_t user_pointer;
        uint64_t offset = (uint64_t)i * sizeof(uint64_t);
        int64_t status;

        if (argv_ptr > UINT64_MAX - offset ||
            sys_copy_from_user(process, &user_pointer, argv_ptr + offset,
                               sizeof(user_pointer)) != 0 ||
            user_pointer == 0U || used >= PANEL_BOOT_ARGV_MAX_BYTES) {
            return ERR_INVAL;
        }

        kernel_argv->offsets[i] = (uint16_t)used;
        status = sys_user_copy_cstr(process, user_pointer,
                                    &kernel_argv->bytes[used],
                                    PANEL_BOOT_ARGV_MAX_BYTES - used);
        if (status != 0) {
            return status;
        }
        while (kernel_argv->bytes[used++] != '\0') {
        }
    }

    kernel_argv->bytes_used = (uint16_t)used;
    return 0;
}
