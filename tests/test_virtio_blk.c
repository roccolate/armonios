#include "unity/unity.h"

#include "storage/virtio_blk.h"

#include <stdint.h>

#define VIRTIO_MMIO_MAGIC_VALUE 0x000U
#define VIRTIO_MMIO_VERSION     0x004U
#define VIRTIO_MMIO_DEVICE_ID   0x008U
#define VIRTIO_MMIO_VENDOR_ID   0x00cU
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034U
#define VIRTIO_MMIO_QUEUE_NUM   0x038U
#define VIRTIO_MMIO_QUEUE_READY 0x044U
#define VIRTIO_MMIO_STATUS      0x070U
#define VIRTIO_MMIO_CONFIG      0x100U

#define VIRTIO_MMIO_MAGIC       0x74726976U

static void write_reg(uint32_t *mmio, uint32_t offset, uint32_t value) {
    mmio[offset / sizeof(uint32_t)] = value;
}

void test_virtio_blk_probe_reads_capacity(void) {
    uint32_t mmio[128] = { 0 };
    virtio_blk_info_t info = { 0 };

    write_reg(mmio, VIRTIO_MMIO_MAGIC_VALUE, VIRTIO_MMIO_MAGIC);
    write_reg(mmio, VIRTIO_MMIO_VERSION, 2);
    write_reg(mmio, VIRTIO_MMIO_DEVICE_ID, 2);
    write_reg(mmio, VIRTIO_MMIO_VENDOR_ID, 0x554d4551U);
    write_reg(mmio, VIRTIO_MMIO_CONFIG, 0x89abcdefU);
    write_reg(mmio, VIRTIO_MMIO_CONFIG + 4U, 0x01234567U);

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)virtio_blk_probe(
                                  (uint64_t)(uintptr_t)mmio, &info));
    TEST_ASSERT_EQUAL_UINT64(2, info.version);
    TEST_ASSERT_EQUAL_UINT64(0x554d4551U, info.vendor_id);
    TEST_ASSERT_EQUAL_UINT64(0x0123456789abcdefULL, info.capacity_sectors);
}

void test_virtio_blk_probe_rejects_non_block_device(void) {
    uint32_t mmio[128] = { 0 };
    virtio_blk_info_t info = { 0 };

    write_reg(mmio, VIRTIO_MMIO_MAGIC_VALUE, VIRTIO_MMIO_MAGIC);
    write_reg(mmio, VIRTIO_MMIO_VERSION, 2);
    write_reg(mmio, VIRTIO_MMIO_DEVICE_ID, 1);

    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)virtio_blk_probe(
                                 (uint64_t)(uintptr_t)mmio, &info));
}

void test_virtio_blk_probe_range_finds_later_transport(void) {
    uint32_t mmio[512] = { 0 };
    uint32_t *transport = &mmio[0x200U / sizeof(uint32_t)];
    virtio_blk_info_t info = { 0 };
    uint64_t found_base = 0;

    write_reg(transport, VIRTIO_MMIO_MAGIC_VALUE, VIRTIO_MMIO_MAGIC);
    write_reg(transport, VIRTIO_MMIO_VERSION, 1);
    write_reg(transport, VIRTIO_MMIO_DEVICE_ID, 2);
    write_reg(transport, VIRTIO_MMIO_CONFIG, 4096);

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)virtio_blk_probe_range(
                                  (uint64_t)(uintptr_t)mmio, 0x400U, 0x200U,
                                  &found_base, &info));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(uintptr_t)transport, found_base);
    TEST_ASSERT_EQUAL_UINT64(4096, info.capacity_sectors);
}

void test_virtio_blk_init_programs_modern_queue(void) {
    uint32_t mmio[128] = { 0 };
    virtio_blk_device_t device = { 0 };

    write_reg(mmio, VIRTIO_MMIO_MAGIC_VALUE, VIRTIO_MMIO_MAGIC);
    write_reg(mmio, VIRTIO_MMIO_VERSION, 2);
    write_reg(mmio, VIRTIO_MMIO_DEVICE_ID, 2);
    write_reg(mmio, VIRTIO_MMIO_QUEUE_NUM_MAX, 8);

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)virtio_blk_init(
                                  &device, (uint64_t)(uintptr_t)mmio));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(uintptr_t)mmio, device.base);
    TEST_ASSERT_EQUAL_UINT64(8, device.queue_size);
    TEST_ASSERT_EQUAL_UINT64(1, device.ready);
    TEST_ASSERT_EQUAL_UINT64(8, mmio[VIRTIO_MMIO_QUEUE_NUM / sizeof(uint32_t)]);
    TEST_ASSERT_EQUAL_UINT64(1, mmio[VIRTIO_MMIO_QUEUE_READY / sizeof(uint32_t)]);
    TEST_ASSERT_EQUAL_UINT64(15, mmio[VIRTIO_MMIO_STATUS / sizeof(uint32_t)]);
}
