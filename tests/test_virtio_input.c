#include "unity/unity.h"

#include "input/input.h"
#include "input/virtio_input.h"

#include <stdint.h>

#define VIRTIO_MMIO_MAGIC_VALUE 0x000U
#define VIRTIO_MMIO_VERSION     0x004U
#define VIRTIO_MMIO_DEVICE_ID   0x008U
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034U
#define VIRTIO_MMIO_QUEUE_NUM   0x038U
#define VIRTIO_MMIO_QUEUE_READY 0x044U
#define VIRTIO_MMIO_STATUS      0x070U

#define VIRTIO_MMIO_MAGIC       0x74726976U

static void write_reg(uint32_t *mmio, uint32_t offset, uint32_t value) {
    mmio[offset / sizeof(uint32_t)] = value;
}

static void setup_input_mmio(uint32_t *mmio, uint32_t queue_size) {
    write_reg(mmio, VIRTIO_MMIO_MAGIC_VALUE, VIRTIO_MMIO_MAGIC);
    write_reg(mmio, VIRTIO_MMIO_VERSION, 2);
    write_reg(mmio, VIRTIO_MMIO_DEVICE_ID, 18);
    write_reg(mmio, VIRTIO_MMIO_QUEUE_NUM_MAX, queue_size);
}

void test_virtio_input_probe_rejects_wrong_device(void) {
    uint32_t mmio[128] = { 0 };

    setup_input_mmio(mmio, 16);
    write_reg(mmio, VIRTIO_MMIO_DEVICE_ID, 2);

    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)virtio_input_probe(
                                 (uint64_t)(uintptr_t)mmio));
}

void test_virtio_input_init_records_negotiated_queue_size(void) {
    uint32_t mmio[128] = { 0 };
    virtio_input_device_t device = { 0 };

    setup_input_mmio(mmio, 8);

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)virtio_input_init(
                                  &device, (uint64_t)(uintptr_t)mmio));
    TEST_ASSERT_EQUAL_UINT64(1, device.ready);
    TEST_ASSERT_EQUAL_UINT64(8, device.queue_size);
    TEST_ASSERT_EQUAL_UINT64(8, mmio[VIRTIO_MMIO_QUEUE_NUM / sizeof(uint32_t)]);
    TEST_ASSERT_EQUAL_UINT64(1, mmio[VIRTIO_MMIO_QUEUE_READY / sizeof(uint32_t)]);
    TEST_ASSERT_EQUAL_UINT64(7, mmio[VIRTIO_MMIO_STATUS / sizeof(uint32_t)]);

    input_queue_init();
    for (uint16_t i = 0; i < device.queue_size; i++) {
        virtio_input_test_set_event(i, i, VIRTIO_INPUT_EV_KEY,
                                    VIRTIO_INPUT_KEY_A, 1);
    }
    virtio_input_test_set_used_idx(10);

    TEST_ASSERT_EQUAL_UINT64(8, (uint64_t)virtio_input_poll(&device));
    TEST_ASSERT_EQUAL_UINT64(8, device.last_used_idx);
    TEST_ASSERT_EQUAL_UINT64(1, virtio_input_has_events(&device));

    TEST_ASSERT_EQUAL_UINT64(2, (uint64_t)virtio_input_poll(&device));
    TEST_ASSERT_EQUAL_UINT64(10, device.last_used_idx);
    TEST_ASSERT_EQUAL_UINT64(0, virtio_input_has_events(&device));
    TEST_ASSERT_EQUAL_UINT64(10, input_queue_available());
}

void test_virtio_input_rejects_invalid_inputs(void) {
    uint32_t mmio[128] = { 0 };
    virtio_input_device_t device = { 0 };

    setup_input_mmio(mmio, 16);

    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)virtio_input_init(0,
                                 (uint64_t)(uintptr_t)mmio));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)virtio_input_init(&device, 0));
    TEST_ASSERT_EQUAL_UINT64(0, virtio_input_has_events(0));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)virtio_input_poll(0));
}

void test_virtio_input_key_translation_maps_ascii_and_navigation(void) {
    TEST_ASSERT_EQUAL_UINT64('a',
        virtio_input_key_to_input_key(VIRTIO_INPUT_KEY_A, 0, 0));
    TEST_ASSERT_EQUAL_UINT64('A',
        virtio_input_key_to_input_key(VIRTIO_INPUT_KEY_A, 1, 0));
    TEST_ASSERT_EQUAL_UINT64(INPUT_KEY_UP,
        virtio_input_key_to_input_key(VIRTIO_INPUT_KEY_UP, 0, 0));
    TEST_ASSERT_EQUAL_UINT64(INPUT_KEY_DELETE,
        virtio_input_key_to_input_key(VIRTIO_INPUT_KEY_DELETE, 0, 0));
}

void test_virtio_input_key_translation_maps_ctrl_letters(void) {
    TEST_ASSERT_EQUAL_UINT64(19,
        virtio_input_key_to_input_key(VIRTIO_INPUT_KEY_S, 0, 1));
    TEST_ASSERT_EQUAL_UINT64(17,
        virtio_input_key_to_input_key(VIRTIO_INPUT_KEY_Q, 1, 1));
}
