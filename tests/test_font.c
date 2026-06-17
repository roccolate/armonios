#include <stdint.h>

#include "unity/unity.h"
#include "../kernel/font.h"

void test_font_draw_char_renders_5x7_glyph(void) {
    uint32_t pixels[64] = { 0 };
    fb_t fb;

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fb_init(&fb, pixels, 8, 8, 8));

    font_draw_char(&fb, 1, 0, 'A', 0xffabcdefU);

    TEST_ASSERT_EQUAL_UINT64(0, pixels[0]);
    TEST_ASSERT_EQUAL_UINT64(0, pixels[1]);
    TEST_ASSERT_EQUAL_UINT64(0xffabcdefU, pixels[2]);
    TEST_ASSERT_EQUAL_UINT64(0xffabcdefU, pixels[3]);
    TEST_ASSERT_EQUAL_UINT64(0xffabcdefU, pixels[4]);
    TEST_ASSERT_EQUAL_UINT64(0xffabcdefU, pixels[1 * 8 + 1]);
    TEST_ASSERT_EQUAL_UINT64(0xffabcdefU, pixels[1 * 8 + 5]);
    TEST_ASSERT_EQUAL_UINT64(0xffabcdefU, pixels[3 * 8 + 1]);
    TEST_ASSERT_EQUAL_UINT64(0xffabcdefU, pixels[3 * 8 + 5]);
    TEST_ASSERT_EQUAL_UINT64(0xffabcdefU, pixels[3 * 8 + 3]);
}

void test_font_draw_text_advances_and_handles_lowercase(void) {
    uint32_t pixels[128] = { 0 };
    fb_t fb;

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fb_init(&fb, pixels, 16, 8, 16));

    font_draw_text(&fb, 0, 0, "a1", 0xff123456U);

    TEST_ASSERT_EQUAL_UINT64(0xff123456U, pixels[1]);
    TEST_ASSERT_EQUAL_UINT64(0xff123456U, pixels[2]);
    TEST_ASSERT_EQUAL_UINT64(0xff123456U, pixels[6 + 2]);
}

void test_font_draw_text_clips_at_framebuffer_edge(void) {
    uint32_t pixels[16] = { 0 };
    fb_t fb;

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fb_init(&fb, pixels, 4, 4, 4));

    font_draw_char(&fb, 2, 0, 'O', 0xff00ff00U);

    TEST_ASSERT_EQUAL_UINT64(0xff00ff00U, pixels[3]);
    TEST_ASSERT_EQUAL_UINT64(0xff00ff00U, pixels[1 * 4 + 2]);
    TEST_ASSERT_EQUAL_UINT64(0, pixels[1 * 4 + 3]);
    TEST_ASSERT_EQUAL_UINT64(0, pixels[0]);
}
