/*
 * test_syscall_helpers.c
 *
 * Direct coverage for the syscall-boundary helpers. These helpers are the
 * single choke point for user-buffer validation and owner-window lookups, so
 * tests here pin the ABI-facing error codes without driving a full SVC frame.
 */

#include "unity/unity.h"

#include <stdint.h>
#include <stdlib.h>

#include "fb/fb.h"
#include "kernel/gui.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/process.h"
#include "kernel/syscall_helpers.h"

#define USER_HELPER_TEST_PAGES 32U

typedef struct {
    void *pmm_memory;
    uint8_t *user_page;
    uint64_t *pgd;
} user_mapping_t;

static uint64_t err64(int64_t value) {
    return (uint64_t)value;
}

static void user_mapping_init(user_mapping_t *mapping, process_t *process,
                              uint32_t pid, const char *name,
                              uint64_t region_size, uint64_t vmm_flags) {
    int rc;

    mapping->pmm_memory = 0;
    mapping->user_page = 0;
    mapping->pgd = 0;

    rc = posix_memalign(&mapping->pmm_memory, PAGE_SIZE,
                        USER_HELPER_TEST_PAGES * PAGE_SIZE);
    if (rc != 0) {
        TEST_FAIL("posix_memalign pmm memory failed");
    }
    pmm_init((uint64_t)(uintptr_t)mapping->pmm_memory,
             USER_HELPER_TEST_PAGES * PAGE_SIZE);

    mapping->pgd = vmm_new_table();
    TEST_ASSERT_NOT_NULL(mapping->pgd);

    rc = posix_memalign((void **)&mapping->user_page, PAGE_SIZE, PAGE_SIZE);
    if (rc != 0) {
        TEST_FAIL("posix_memalign user page failed");
    }
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        mapping->user_page[i] = 0;
    }

    process_init(process, pid, name);
    process_set_page_table(process, mapping->pgd);
    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)process_add_user_region(
               process, (uint64_t)(uintptr_t)mapping->user_page, region_size));
    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)vmm_map_page(
               mapping->pgd, (uint64_t)(uintptr_t)mapping->user_page,
               (uint64_t)(uintptr_t)mapping->user_page,
               vmm_flags | VMM_FLAG_USER));
}

static void user_mapping_destroy(user_mapping_t *mapping) {
    free(mapping->user_page);
    free(mapping->pmm_memory);
    mapping->user_page = 0;
    mapping->pmm_memory = 0;
    mapping->pgd = 0;
}

void test_syscall_helpers_user_buffers_validate_registered_ranges(void) {
    process_t writable_process;
    process_t readonly_process;
    user_mapping_t writable;
    user_mapping_t readonly;
    uint8_t kernel_input[4] = {1, 2, 3, 4};
    uint8_t kernel_output[4] = {0};
    uint64_t writable_base;
    uint64_t readonly_base;

    user_mapping_init(&writable, &writable_process, 100U, "buf-rw", PAGE_SIZE,
                      VMM_FLAG_READ | VMM_FLAG_WRITE);
    writable_base = (uint64_t)(uintptr_t)writable.user_page;

    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_user_buf_in(&writable_process, writable_base,
                                     PAGE_SIZE));
    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_user_buf_out(&writable_process, writable_base + 4U,
                                      4U));
    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_user_buf_in(&writable_process, 0, 0));

    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_copy_to_user(&writable_process, writable_base,
                                      kernel_input, sizeof(kernel_input)));
    TEST_ASSERT_EQUAL_UINT64(1, writable.user_page[0]);
    TEST_ASSERT_EQUAL_UINT64(4, writable.user_page[3]);
    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_copy_from_user(&writable_process, kernel_output,
                                        writable_base, sizeof(kernel_output)));
    TEST_ASSERT_EQUAL_UINT64(1, kernel_output[0]);
    TEST_ASSERT_EQUAL_UINT64(4, kernel_output[3]);

    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_INVAL),
        (uint64_t)sys_user_buf_in(0, writable_base, 1U));
    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_INVAL),
        (uint64_t)sys_user_buf_in(&writable_process, 0, 1U));
    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_INVAL),
        (uint64_t)sys_user_buf_out(&writable_process,
                                   writable_base + PAGE_SIZE - 1U, 2U));

    user_mapping_init(&readonly, &readonly_process, 101U, "buf-ro", PAGE_SIZE,
                      VMM_FLAG_READ | VMM_FLAG_EXEC);
    readonly_base = (uint64_t)(uintptr_t)readonly.user_page;
    readonly.user_page[0] = 0x5aU;

    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_user_buf_in(&readonly_process, readonly_base, 1U));
    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_PERM),
        (uint64_t)sys_user_buf_out(&readonly_process, readonly_base, 1U));
    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_PERM),
        (uint64_t)sys_copy_to_user(&readonly_process, readonly_base,
                                   kernel_input, 1U));
    TEST_ASSERT_EQUAL_UINT64(0x5aU, readonly.user_page[0]);

    user_mapping_destroy(&readonly);
    user_mapping_destroy(&writable);
}

void test_syscall_helpers_copy_cstr_validates_each_byte(void) {
    process_t process;
    user_mapping_t mapping;
    char out[8] = {0};
    uint64_t base;

    user_mapping_init(&mapping, &process, 102U, "cstr", PAGE_SIZE,
                      VMM_FLAG_READ | VMM_FLAG_WRITE);
    base = (uint64_t)(uintptr_t)mapping.user_page;
    mapping.user_page[0] = 'd';
    mapping.user_page[1] = 'e';
    mapping.user_page[2] = 's';
    mapping.user_page[3] = 'k';
    mapping.user_page[4] = '\0';

    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_user_copy_cstr(&process, base, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT64('d', (uint64_t)out[0]);
    TEST_ASSERT_EQUAL_UINT64('e', (uint64_t)out[1]);
    TEST_ASSERT_EQUAL_UINT64('s', (uint64_t)out[2]);
    TEST_ASSERT_EQUAL_UINT64('k', (uint64_t)out[3]);
    TEST_ASSERT_EQUAL_UINT64('\0', (uint64_t)out[4]);

    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_INVAL),
        (uint64_t)sys_user_copy_cstr(&process, base, out, 2U));
    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_INVAL),
        (uint64_t)sys_user_copy_cstr(&process, 0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_INVAL),
        (uint64_t)sys_user_copy_cstr(&process, base, 0, sizeof(out)));

    user_mapping_destroy(&mapping);
}

void test_syscall_helpers_copy_cstr_rejects_unregistered_tail(void) {
    process_t process;
    user_mapping_t mapping;
    char out[8] = {0};
    uint64_t base;

    user_mapping_init(&mapping, &process, 103U, "tail", 2U,
                      VMM_FLAG_READ | VMM_FLAG_WRITE);
    base = (uint64_t)(uintptr_t)mapping.user_page;
    mapping.user_page[0] = 'a';
    mapping.user_page[1] = 'b';
    mapping.user_page[2] = 'c';
    mapping.user_page[3] = '\0';

    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_INVAL),
        (uint64_t)sys_user_copy_cstr(&process, base, out, sizeof(out)));

    user_mapping_destroy(&mapping);
}

void test_syscall_helpers_owner_window_error_modes_are_stable(void) {
    uint32_t pixels[64 * 64] = {0};
    fb_t fb;
    gui_desktop_t *desktop;
    gui_desktop_t *out_desktop = 0;
    gui_window_t *out_window = 0;
    process_t owner;
    process_t other;
    uint32_t window_id = GUI_NO_WINDOW;
    uint32_t missing_id = GUI_MAX_WINDOWS - 1U;

    fb_init(&fb, pixels, 64, 64, 64);
    gui_init_for_framebuffer(&fb, 0);
    desktop = gui_desktop();
    TEST_ASSERT_NOT_NULL(desktop);

    process_init(&owner, 7U, "owner");
    process_init(&other, 8U, "other");

    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)gui_create_window_for_pid(
                                 desktop, owner.pid, 0, 0, 16, 16,
                                 0xff000000U, 0xffffffffU, "owned",
                                 &window_id));
    TEST_ASSERT_TRUE(window_id < GUI_MAX_WINDOWS);
    if (missing_id == window_id) {
        missing_id--;
    }

    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)sys_owner_window(
                                 &owner, window_id, &out_desktop,
                                 &out_window));
    TEST_ASSERT_TRUE(out_desktop == desktop);
    TEST_ASSERT_TRUE(out_window == &desktop->windows[window_id]);

    TEST_ASSERT_EQUAL_UINT64(err64(ERR_NOENT),
                             (uint64_t)sys_owner_window(
                                 &owner, missing_id, &out_desktop,
                                 &out_window));
    TEST_ASSERT_EQUAL_UINT64(err64(ERR_BADF),
                             (uint64_t)sys_owner_window_badf(
                                 &owner, missing_id, &out_desktop,
                                 &out_window));
    TEST_ASSERT_EQUAL_UINT64(err64(ERR_BADF),
                             (uint64_t)sys_owner_window(
                                 &other, window_id, &out_desktop,
                                 &out_window));
    TEST_ASSERT_EQUAL_UINT64(err64(ERR_INVAL),
                             (uint64_t)sys_owner_window(&owner, window_id, 0,
                                                        0));
}
