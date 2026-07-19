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

#define USER_HELPER_TEST_PAGES 64U

typedef struct {
    void *pmm_memory;
    uint8_t *user_pages;
    uint64_t *pgd;
    uint32_t page_count;
} user_mapping_t;

static uint64_t err64(int64_t value) {
    return (uint64_t)value;
}

static void user_mapping_init(user_mapping_t *mapping, process_t *process,
                              uint32_t pid, const char *name,
                              uint32_t page_count, uint64_t region_size) {
    int rc;

    mapping->pmm_memory = 0;
    mapping->user_pages = 0;
    mapping->pgd = 0;
    mapping->page_count = page_count;

    rc = posix_memalign(&mapping->pmm_memory, PAGE_SIZE,
                        USER_HELPER_TEST_PAGES * PAGE_SIZE);
    if (rc != 0) {
        TEST_FAIL("posix_memalign pmm memory failed");
    }
    pmm_init((uint64_t)(uintptr_t)mapping->pmm_memory,
             USER_HELPER_TEST_PAGES * PAGE_SIZE);

    mapping->pgd = vmm_new_table();
    TEST_ASSERT_NOT_NULL(mapping->pgd);

    rc = posix_memalign((void **)&mapping->user_pages, PAGE_SIZE,
                        (size_t)page_count * PAGE_SIZE);
    if (rc != 0) {
        TEST_FAIL("posix_memalign user pages failed");
    }
    for (uint64_t i = 0; i < (uint64_t)page_count * PAGE_SIZE; i++) {
        mapping->user_pages[i] = 0;
    }

    process_init(process, pid, name);
    process_set_page_table(process, mapping->pgd);
    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)process_add_user_region(
               process, (uint64_t)(uintptr_t)mapping->user_pages, region_size));
}

static void user_mapping_map_page(user_mapping_t *mapping, uint32_t index,
                                  uint64_t vmm_flags) {
    uint64_t address;

    TEST_ASSERT_TRUE(index < mapping->page_count);
    address = (uint64_t)(uintptr_t)mapping->user_pages +
              (uint64_t)index * PAGE_SIZE;
    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)vmm_map_page(mapping->pgd, address, address,
                                  vmm_flags | VMM_FLAG_USER));
}

static void user_mapping_destroy(user_mapping_t *mapping) {
    if (mapping->pgd != 0) {
        vmm_free_table(mapping->pgd);
    }
    free(mapping->user_pages);
    free(mapping->pmm_memory);
    mapping->user_pages = 0;
    mapping->pmm_memory = 0;
    mapping->pgd = 0;
    mapping->page_count = 0;
}

void test_syscall_helpers_user_buffers_validate_registered_ranges(void) {
    process_t process;
    user_mapping_t mapping;
    uint8_t kernel_input[4] = {1, 2, 3, 4};
    uint8_t kernel_output[4] = {0};
    uint64_t base;

    user_mapping_init(&mapping, &process, 100U, "buf-rw", 1U, PAGE_SIZE);
    user_mapping_map_page(&mapping, 0U, VMM_FLAG_READ | VMM_FLAG_WRITE);
    base = (uint64_t)(uintptr_t)mapping.user_pages;

    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_user_buf_in(&process, base, PAGE_SIZE));
    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_user_buf_out(&process, base + 4U, 4U));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)sys_user_buf_in(&process, 0, 0));

    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_copy_to_user(&process, base, kernel_input,
                                      sizeof(kernel_input)));
    TEST_ASSERT_EQUAL_UINT64(1, mapping.user_pages[0]);
    TEST_ASSERT_EQUAL_UINT64(4, mapping.user_pages[3]);
    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_copy_from_user(&process, kernel_output, base,
                                        sizeof(kernel_output)));
    TEST_ASSERT_EQUAL_UINT64(1, kernel_output[0]);
    TEST_ASSERT_EQUAL_UINT64(4, kernel_output[3]);

    TEST_ASSERT_EQUAL_UINT64(err64(ERR_INVAL),
                             (uint64_t)sys_user_buf_in(0, base, 1U));
    TEST_ASSERT_EQUAL_UINT64(err64(ERR_INVAL),
                             (uint64_t)sys_user_buf_in(&process, 0, 1U));
    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_INVAL),
        (uint64_t)sys_user_buf_out(&process, base + PAGE_SIZE - 1U, 2U));
    user_mapping_destroy(&mapping);

    user_mapping_init(&mapping, &process, 101U, "buf-ro", 1U, PAGE_SIZE);
    user_mapping_map_page(&mapping, 0U, VMM_FLAG_READ | VMM_FLAG_EXEC);
    base = (uint64_t)(uintptr_t)mapping.user_pages;
    mapping.user_pages[0] = 0x5aU;

    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)sys_user_buf_in(&process, base, 1U));
    TEST_ASSERT_EQUAL_UINT64(err64(ERR_PERM),
                             (uint64_t)sys_user_buf_out(&process, base, 1U));
    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_PERM),
        (uint64_t)sys_copy_to_user(&process, base, kernel_input, 1U));
    TEST_ASSERT_EQUAL_UINT64(0x5aU, mapping.user_pages[0]);
    user_mapping_destroy(&mapping);

    /*
     * The first page is writable and the second page is read-only. The copy
     * must reject the entire range before mutating the writable prefix.
     */
    user_mapping_init(&mapping, &process, 102U, "buf-mixed", 2U,
                      2U * PAGE_SIZE);
    user_mapping_map_page(&mapping, 0U, VMM_FLAG_READ | VMM_FLAG_WRITE);
    user_mapping_map_page(&mapping, 1U, VMM_FLAG_READ | VMM_FLAG_EXEC);
    base = (uint64_t)(uintptr_t)mapping.user_pages;
    mapping.user_pages[PAGE_SIZE - 2U] = 0xa1U;
    mapping.user_pages[PAGE_SIZE - 1U] = 0xa2U;
    mapping.user_pages[PAGE_SIZE] = 0xb1U;
    mapping.user_pages[PAGE_SIZE + 1U] = 0xb2U;

    TEST_ASSERT_EQUAL_UINT64(
        err64(ERR_PERM),
        (uint64_t)sys_copy_to_user(&process, base + PAGE_SIZE - 2U,
                                   kernel_input, sizeof(kernel_input)));
    TEST_ASSERT_EQUAL_UINT64(0xa1U, mapping.user_pages[PAGE_SIZE - 2U]);
    TEST_ASSERT_EQUAL_UINT64(0xa2U, mapping.user_pages[PAGE_SIZE - 1U]);
    TEST_ASSERT_EQUAL_UINT64(0xb1U, mapping.user_pages[PAGE_SIZE]);
    TEST_ASSERT_EQUAL_UINT64(0xb2U, mapping.user_pages[PAGE_SIZE + 1U]);
    user_mapping_destroy(&mapping);
}

void test_syscall_helpers_copy_cstr_validates_each_byte(void) {
    process_t process;
    user_mapping_t mapping;
    char out[8] = {0};
    uint64_t base;

    user_mapping_init(&mapping, &process, 103U, "cstr", 1U, PAGE_SIZE);
    user_mapping_map_page(&mapping, 0U, VMM_FLAG_READ | VMM_FLAG_WRITE);
    base = (uint64_t)(uintptr_t)mapping.user_pages;
    mapping.user_pages[0] = 'd';
    mapping.user_pages[1] = 'e';
    mapping.user_pages[2] = 's';
    mapping.user_pages[3] = 'k';
    mapping.user_pages[4] = '\0';

    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)sys_user_copy_cstr(&process, base, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT64('d', (uint64_t)out[0]);
    TEST_ASSERT_EQUAL_UINT64('e', (uint64_t)out[1]);
    TEST_ASSERT_EQUAL_UINT64('s', (uint64_t)out[2]);
    TEST_ASSERT_EQUAL_UINT64('k', (uint64_t)out[3]);
    TEST_ASSERT_EQUAL_UINT64('\0', (uint64_t)out[4]);

    TEST_ASSERT_EQUAL_UINT64(err64(ERR_INVAL),
                             (uint64_t)sys_user_copy_cstr(&process, base, out,
                                                          2U));
    TEST_ASSERT_EQUAL_UINT64(err64(ERR_INVAL),
                             (uint64_t)sys_user_copy_cstr(&process, 0, out,
                                                          sizeof(out)));
    TEST_ASSERT_EQUAL_UINT64(err64(ERR_INVAL),
                             (uint64_t)sys_user_copy_cstr(&process, base, 0,
                                                          sizeof(out)));

    user_mapping_destroy(&mapping);
}

void test_syscall_helpers_copy_cstr_rejects_unregistered_tail(void) {
    process_t process;
    user_mapping_t mapping;
    char out[8] = {0};
    uint64_t base;

    user_mapping_init(&mapping, &process, 104U, "tail", 1U, 2U);
    user_mapping_map_page(&mapping, 0U, VMM_FLAG_READ | VMM_FLAG_WRITE);
    base = (uint64_t)(uintptr_t)mapping.user_pages;
    mapping.user_pages[0] = 'a';
    mapping.user_pages[1] = 'b';
    mapping.user_pages[2] = 'c';
    mapping.user_pages[3] = '\0';

    TEST_ASSERT_EQUAL_UINT64(err64(ERR_INVAL),
                             (uint64_t)sys_user_copy_cstr(&process, base, out,
                                                          sizeof(out)));

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
