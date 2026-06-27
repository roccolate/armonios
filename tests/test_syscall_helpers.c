/*
 * test_syscall_helpers.c
 *
 * Direct coverage for the syscall-boundary helpers. These helpers are the
 * single choke point for user-buffer validation and owner-window lookups, so
 * tests here pin the ABI-facing error codes without driving a full SVC frame.
 */

#include "unity/unity.h"

#include <stdint.h>

#include "fb/fb.h"
#include "kernel/gui.h"
#include "kernel/process.h"
#include "kernel/syscall_helpers.h"

static uint64_t err64(int64_t value) {
    return (uint64_t)value;
}

void test_syscall_helpers_user_buffers_validate_registered_ranges(void) {
    process_t process;
    uint8_t bytes[8] = {0};
    uint64_t base = (uint64_t)(uintptr_t)bytes;

    process_init(&process, 100U, "buf");
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)process_add_user_region(
                                 &process, base, sizeof(bytes)));

    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)sys_user_buf_in(&process, base,
                                                       sizeof(bytes)));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)sys_user_buf_out(&process, base + 4U,
                                                        4U));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)sys_user_buf_in(&process, 0, 0));

    TEST_ASSERT_EQUAL_UINT64(err64(ERR_INVAL),
                             (uint64_t)sys_user_buf_in(0, base, 1U));
    TEST_ASSERT_EQUAL_UINT64(err64(ERR_INVAL),
                             (uint64_t)sys_user_buf_in(&process, 0, 1U));
    TEST_ASSERT_EQUAL_UINT64(err64(ERR_INVAL),
                             (uint64_t)sys_user_buf_out(&process, base + 7U,
                                                        2U));
}

void test_syscall_helpers_copy_cstr_validates_each_byte(void) {
    process_t process;
    char input[] = "desk";
    char out[8] = {0};
    uint64_t base = (uint64_t)(uintptr_t)input;

    process_init(&process, 101U, "cstr");
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)process_add_user_region(
                                 &process, base, sizeof(input)));

    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)sys_user_copy_cstr(&process, base, out,
                                                          sizeof(out)));
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
}

void test_syscall_helpers_copy_cstr_rejects_unregistered_tail(void) {
    process_t process;
    char input[] = "abcd";
    char out[8] = {0};
    uint64_t base = (uint64_t)(uintptr_t)input;

    process_init(&process, 102U, "tail");
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)process_add_user_region(&process, base,
                                                               2U));

    TEST_ASSERT_EQUAL_UINT64(err64(ERR_INVAL),
                             (uint64_t)sys_user_copy_cstr(&process, base, out,
                                                          sizeof(out)));
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
