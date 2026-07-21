#include "unity/unity.h"
#include "usb/hid_driver.h"
#include "usb/usb_core.h"

#include <stdint.h>

/*
 * Tests for the kernel-wide HID state: reset and poll-all.
 *
 * The poll loop iterates registered devices and calls usb_hid_poll_device,
 * which in turn calls into the bus driver. Host tests use empty endpoint
 * records, but still verify reset bookkeeping and the fixed scan bound.
 */

void test_hid_state_reset_clears_count(void) {
    usb_hid_state_reset();
    TEST_ASSERT_EQUAL_UINT64(0, g_usb_hid_state.count);
}

void test_hid_state_reset_clears_device_endpoints(void) {
    usb_hid_state_reset();
    g_usb_hid_state.devices[0].endpoint_in = 0x81U;
    g_usb_hid_state.devices[0].interface_number = 3U;
    g_usb_hid_state.devices[0].interval = 10U;
    g_usb_hid_state.devices[0].protocol = 0x01U;
    g_usb_hid_state.devices[0].prev_buttons = 0xFFU;
    usb_hid_state_reset();
    TEST_ASSERT_EQUAL_UINT64(0, g_usb_hid_state.devices[0].endpoint_in);
    TEST_ASSERT_EQUAL_UINT64(0, g_usb_hid_state.devices[0].interface_number);
    TEST_ASSERT_EQUAL_UINT64(0, g_usb_hid_state.devices[0].interval);
    TEST_ASSERT_EQUAL_UINT64(0, g_usb_hid_state.devices[0].protocol);
    TEST_ASSERT_EQUAL_UINT64(0, g_usb_hid_state.devices[0].prev_buttons);
}

void test_hid_poll_all_returns_zero_on_empty_state(void) {
    usb_hid_state_reset();
    int n = usb_hid_poll_all();
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)n);
    TEST_ASSERT_EQUAL_UINT64(0, usb_hid_test_poll_iterations());

    /* A corrupted public count must not scan beyond the fixed device array. */
    g_usb_hid_state.count = UINT8_MAX;
    n = usb_hid_poll_all();
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)n);
    TEST_ASSERT_EQUAL_UINT64(USB_HID_POLL_BUDGET,
                             usb_hid_test_poll_iterations());
    usb_hid_state_reset();
}

void test_hid_state_count_field_is_accessible(void) {
    /* The state object is exposed; the field is uint8_t. */
    usb_hid_state_reset();
    TEST_ASSERT_EQUAL_UINT64(0, g_usb_hid_state.count);
    g_usb_hid_state.count = 2;
    TEST_ASSERT_EQUAL_UINT64(2, g_usb_hid_state.count);
    g_usb_hid_state.count = 0;
}

void test_hid_max_devices_constant_is_sane(void) {
    TEST_ASSERT_EQUAL_UINT64(4, USB_HID_MAX_DEVICES);
    TEST_ASSERT_EQUAL_UINT64(USB_HID_MAX_DEVICES, USB_HID_POLL_BUDGET);
}
