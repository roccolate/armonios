#include <stdint.h>

#include "unity/unity.h"
#include "../kernel/gui.h"

void test_gui_create_draws_windows_in_creation_order(void) {
    uint32_t pixels[64] = { 0 };
    fb_t fb;
    gui_desktop_t desktop;
    uint32_t first;
    uint32_t second;

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fb_init(&fb, pixels, 8, 8, 8));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)gui_init(&desktop, &fb, 0xff101010U));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)gui_create_window(
                                 &desktop, 1, 1, 5, 5, 0xff0000aaU,
                                 0xffffffffU, &first));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)gui_create_window(
                                 &desktop, 3, 3, 4, 4, 0xff00aa00U,
                                 0xffeeee00U, &second));

    gui_draw(&desktop);

    TEST_ASSERT_EQUAL_UINT64(0xff101010U, pixels[0]);
    TEST_ASSERT_EQUAL_UINT64(0xffffffffU, pixels[1 * 8 + 1]);
    TEST_ASSERT_EQUAL_UINT64(0xff0000aaU, pixels[2 * 8 + 2]);
    TEST_ASSERT_EQUAL_UINT64(0xffeeee00U, pixels[3 * 8 + 3]);
    TEST_ASSERT_EQUAL_UINT64(0xff00aa00U, pixels[4 * 8 + 4]);
    TEST_ASSERT_EQUAL_UINT64(0, first);
    TEST_ASSERT_EQUAL_UINT64(1, second);
}

void test_gui_move_window_redraws_at_new_position(void) {
    uint32_t pixels[36] = { 0 };
    fb_t fb;
    gui_desktop_t desktop;
    uint32_t window_id;

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fb_init(&fb, pixels, 6, 6, 6));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)gui_init(&desktop, &fb, 0xff111111U));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)gui_create_window(
                                 &desktop, 1, 1, 3, 3, 0xff222222U,
                                 0xff333333U, &window_id));

    gui_draw(&desktop);
    TEST_ASSERT_EQUAL_UINT64(0xff333333U, pixels[1 * 6 + 1]);

    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)gui_move_window(&desktop, window_id,
                                                       2, 2));
    gui_draw(&desktop);
    TEST_ASSERT_EQUAL_UINT64(0xff111111U, pixels[1 * 6 + 1]);
    TEST_ASSERT_EQUAL_UINT64(0xff333333U, pixels[2 * 6 + 2]);
    TEST_ASSERT_EQUAL_UINT64(0xff222222U, pixels[3 * 6 + 3]);
}

void test_gui_rejects_invalid_inputs_and_window_limit(void) {
    uint32_t pixels[16] = { 0 };
    fb_t fb;
    gui_desktop_t desktop;
    uint32_t id;

    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)gui_init(0, &fb, 0));
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fb_init(&fb, pixels, 4, 4, 4));
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)gui_init(&desktop, &fb, 0));

    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)gui_create_window(
                                 &desktop, 0, 0, 1, 3, 0, 0, &id));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)gui_move_window(&desktop, 0, 1, 1));

    for (uint32_t i = 0; i < GUI_MAX_WINDOWS; i++) {
        TEST_ASSERT_EQUAL_UINT64(0,
                                 (uint64_t)gui_create_window(
                                     &desktop, 0, 0, 2, 2, 0, 0, &id));
    }

    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)gui_create_window(
                                 &desktop, 0, 0, 2, 2, 0, 0, &id));
}

void test_gui_delivers_key_to_focused_window(void) {
    uint32_t pixels[16] = { 0 };
    fb_t fb;
    gui_desktop_t desktop;
    uint32_t first;
    uint32_t second;

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fb_init(&fb, pixels, 4, 4, 4));
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)gui_init(&desktop, &fb, 0));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)gui_create_window(
                                 &desktop, 0, 0, 2, 2, 0, 0, &first));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)gui_create_window(
                                 &desktop, 1, 1, 2, 2, 0, 0, &second));

    TEST_ASSERT_EQUAL_UINT64(first, desktop.focused_window_id);
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)gui_focus_window(&desktop, second));
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)gui_send_key(&desktop, 'k'));

    TEST_ASSERT_EQUAL_UINT64(0, desktop.windows[first].key_count);
    TEST_ASSERT_EQUAL_UINT64(1, desktop.windows[second].key_count);
    TEST_ASSERT_EQUAL_UINT64('k', desktop.windows[second].last_key);
}
