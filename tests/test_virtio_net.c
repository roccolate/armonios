#include "unity/unity.h"

#include "net/virtio_net.h"

#include <stdint.h>

#define VIRTIO_MMIO_MAGIC_VALUE 0x000U
#define VIRTIO_MMIO_VERSION     0x004U
#define VIRTIO_MMIO_DEVICE_ID   0x008U
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020U
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024U
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034U
#define VIRTIO_MMIO_QUEUE_NUM   0x038U
#define VIRTIO_MMIO_QUEUE_READY 0x044U
#define VIRTIO_MMIO_STATUS      0x070U
#define VIRTIO_MMIO_CONFIG      0x100U

#define VIRTIO_MMIO_MAGIC       0x74726976U
#define VIRTIO_F_VERSION_1_HIGH 1U

static void write_reg(uint32_t *mmio, uint32_t offset, uint32_t value) {
    mmio[offset / sizeof(uint32_t)] = value;
}

void test_virtio_net_probe_reads_mac_address(void) {
    uint32_t mmio[128] = { 0 };
    volatile uint8_t *config =
        (volatile uint8_t *)((uintptr_t)mmio + VIRTIO_MMIO_CONFIG);
    virtio_net_info_t info = { 0 };

    write_reg(mmio, VIRTIO_MMIO_MAGIC_VALUE, VIRTIO_MMIO_MAGIC);
    write_reg(mmio, VIRTIO_MMIO_VERSION, 2);
    write_reg(mmio, VIRTIO_MMIO_DEVICE_ID, 1);
    config[0] = 0x02U;
    config[1] = 0xaaU;
    config[2] = 0xbbU;
    config[3] = 0xccU;
    config[4] = 0xddU;
    config[5] = 0xeeU;

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)virtio_net_probe(
                                  (uint64_t)(uintptr_t)mmio, &info));
    TEST_ASSERT_EQUAL_UINT64(0x02U, info.mac[0]);
    TEST_ASSERT_EQUAL_UINT64(0xeeU, info.mac[5]);
}

void test_virtio_net_init_negotiates_feature_banks(void) {
    uint32_t mmio[128] = { 0 };
    volatile uint8_t *config =
        (volatile uint8_t *)((uintptr_t)mmio + VIRTIO_MMIO_CONFIG);
    virtio_net_device_t device = { 0 };

    write_reg(mmio, VIRTIO_MMIO_MAGIC_VALUE, VIRTIO_MMIO_MAGIC);
    write_reg(mmio, VIRTIO_MMIO_VERSION, 2);
    write_reg(mmio, VIRTIO_MMIO_DEVICE_ID, 1);
    write_reg(mmio, VIRTIO_MMIO_QUEUE_NUM_MAX, 32);
    config[0] = 0x02U;

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)virtio_net_init(
                                  &device, (uint64_t)(uintptr_t)mmio));
    TEST_ASSERT_EQUAL_UINT64(1, device.ready);
    TEST_ASSERT_EQUAL_UINT64(16, device.queue_size);
    TEST_ASSERT_EQUAL_UINT64(16, virtio_net_test_rx_available_idx());
    TEST_ASSERT_EQUAL_UINT64(15, mmio[VIRTIO_MMIO_STATUS / sizeof(uint32_t)]);
    TEST_ASSERT_EQUAL_UINT64(1, mmio[VIRTIO_MMIO_QUEUE_READY / sizeof(uint32_t)]);
    TEST_ASSERT_EQUAL_UINT64(1, mmio[VIRTIO_MMIO_DRIVER_FEATURES_SEL /
                                     sizeof(uint32_t)]);
    TEST_ASSERT_EQUAL_UINT64(VIRTIO_F_VERSION_1_HIGH,
                             mmio[VIRTIO_MMIO_DRIVER_FEATURES /
                                  sizeof(uint32_t)]);
    TEST_ASSERT_EQUAL_UINT64(0x02U, device.mac[0]);
}

void test_virtio_net_tx_uses_single_frame_buffer(void) {
    TEST_ASSERT_EQUAL_UINT64(1548U, virtio_net_test_tx_buffer_bytes());
    TEST_ASSERT_EQUAL_UINT64(16U * 1548U, virtio_net_test_rx_buffer_bytes());
}

void test_virtio_net_ready_ops_reject_invalid_device_state(void) {
    virtio_net_device_t device = {
        .base = 0,
        .queue_size = 0,
        .ready = 1,
    };
    uint8_t frame[1] = { 0 };

    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)virtio_net_send(&device, frame,
                                                       sizeof(frame)));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)virtio_net_recv(&device, frame,
                                                       sizeof(frame)));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)virtio_net_poll(&device));
}
