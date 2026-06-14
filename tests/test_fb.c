#include <stdint.h>

#include "unity/unity.h"
#include "../drivers/fb/fb.h"

void test_fb_init_rejects_invalid_geometry(void) {
    fb_t fb;
    uint32_t pixels[4];

    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)fb_init(0, pixels, 2, 2, 2));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)fb_init(&fb, 0, 2, 2, 2));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)fb_init(&fb, pixels, 2, 2, 1));
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fb_init(&fb, pixels, 2, 2, 2));
}

void test_fb_putpixel_and_fillrect_clip_to_bounds(void) {
    fb_t fb;
    uint32_t pixels[16] = {0};

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fb_init(&fb, pixels, 4, 4, 4));

    fb_putpixel(&fb, 1, 2, 0x11223344U);
    TEST_ASSERT_EQUAL_UINT64(0x11223344U, pixels[2 * 4 + 1]);

    fb_putpixel(&fb, 4, 2, 0xffffffffU);
    TEST_ASSERT_EQUAL_UINT64(0, pixels[2 * 4 + 3]);

    fb_fillrect(&fb, 2, 1, 4, 4, 0xaabbccddU);
    TEST_ASSERT_EQUAL_UINT64(0xaabbccddU, pixels[1 * 4 + 2]);
    TEST_ASSERT_EQUAL_UINT64(0xaabbccddU, pixels[1 * 4 + 3]);
    TEST_ASSERT_EQUAL_UINT64(0xaabbccddU, pixels[3 * 4 + 2]);
    TEST_ASSERT_EQUAL_UINT64(0xaabbccddU, pixels[3 * 4 + 3]);
    TEST_ASSERT_EQUAL_UINT64(0, pixels[1 * 4 + 1]);
}

void test_fb_blit_copies_and_clips_source_pixels(void) {
    fb_t fb;
    uint32_t pixels[9] = {0};
    const uint32_t src[4] = {
        0x1U, 0x2U,
        0x3U, 0x4U,
    };

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fb_init(&fb, pixels, 3, 3, 3));

    fb_blit(&fb, 1, 1, src, 2, 2);
    TEST_ASSERT_EQUAL_UINT64(0x1U, pixels[1 * 3 + 1]);
    TEST_ASSERT_EQUAL_UINT64(0x2U, pixels[1 * 3 + 2]);
    TEST_ASSERT_EQUAL_UINT64(0x3U, pixels[2 * 3 + 1]);
    TEST_ASSERT_EQUAL_UINT64(0x4U, pixels[2 * 3 + 2]);
}
