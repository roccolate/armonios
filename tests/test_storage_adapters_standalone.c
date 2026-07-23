#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "storage/emmc_block_device.h"
#include "storage/virtio_blk_block_device.h"

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "assert failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

#define ASSERT_EQ_U64(expected, actual) do { \
    uint64_t expected_ = (uint64_t)(expected); \
    uint64_t actual_ = (uint64_t)(actual); \
    if (expected_ != actual_) { \
        fprintf(stderr, "assert failed: %s:%d: expected %llu got %llu\n", \
                __FILE__, __LINE__, \
                (unsigned long long)expected_, \
                (unsigned long long)actual_); \
        return 1; \
    } \
} while (0)

static uint64_t g_first_block;
static uint32_t g_block_count;
static uint32_t g_read_calls;
static uint32_t g_write_calls;

int virtio_blk_read_sector(virtio_blk_device_t *device, uint64_t sector,
                           void *buffer) {
    (void)device;
    memset(buffer, (int)(sector & 0xffU),
           VIRTIO_BLK_BLOCK_DEVICE_BLOCK_SIZE);
    g_first_block = sector;
    g_block_count++;
    g_read_calls++;
    return 0;
}

int virtio_blk_write_sector(virtio_blk_device_t *device, uint64_t sector,
                            const void *buffer) {
    (void)device;
    (void)buffer;
    g_first_block = sector;
    g_block_count++;
    g_write_calls++;
    return 0;
}

int emmc_read_sector(emmc_device_t *device, uint32_t lba, uint32_t count,
                     void *buffer) {
    (void)device;
    memset(buffer, (int)(lba & 0xffU), (uint64_t)count * EMMC_BLKSZ);
    g_first_block = lba;
    g_block_count = count;
    g_read_calls++;
    return EMMC_OK;
}

int emmc_write_sector(emmc_device_t *device, uint32_t lba, uint32_t count,
                      const void *buffer) {
    (void)device;
    (void)lba;
    (void)count;
    (void)buffer;
    return EMMC_ERR_READ_ONLY;
}

static void set_csd_bits(uint32_t response[4], uint32_t start,
                         uint32_t size, uint32_t value) {
    for (uint32_t bit = 0; bit < size; bit++) {
        uint32_t position = start + bit;
        uint32_t offset = 3U - position / 32U;
        uint32_t mask = 1U << (position & 31U);

        if ((value & (1U << bit)) != 0U) {
            response[offset] |= mask;
        } else {
            response[offset] &= ~mask;
        }
    }
}

static void normalized_csd_to_raw(const uint32_t response[4],
                                  uint32_t raw[4]) {
    raw[0] = response[3] >> 8U;
    raw[1] = (response[2] >> 8U) | (response[3] << 24U);
    raw[2] = (response[1] >> 8U) | (response[2] << 24U);
    raw[3] = (response[0] >> 8U) | (response[1] << 24U);
}

static void build_high_capacity_csd(uint32_t raw[4], uint32_t c_size) {
    uint32_t response[4] = {0};

    set_csd_bits(response, 126U, 2U, 1U);
    set_csd_bits(response, 48U, 22U, c_size);
    normalized_csd_to_raw(response, raw);
}

static void build_standard_capacity_csd(uint32_t raw[4], uint32_t c_size,
                                        uint32_t c_size_mult,
                                        uint32_t read_block_len) {
    uint32_t response[4] = {0};

    set_csd_bits(response, 126U, 2U, 0U);
    set_csd_bits(response, 80U, 4U, read_block_len);
    set_csd_bits(response, 62U, 12U, c_size);
    set_csd_bits(response, 47U, 3U, c_size_mult);
    normalized_csd_to_raw(response, raw);
}

static int test_virtio_adapter(void) {
    virtio_blk_device_t transport = {0};
    block_device_t device;
    uint8_t buffer[VIRTIO_BLK_BLOCK_DEVICE_BLOCK_SIZE * 2U];

    transport.ready = 1U;
    ASSERT_TRUE(virtio_blk_block_device_init(&device, &transport, 4096U) == 0);
    ASSERT_EQ_U64(4096U, device.block_count);
    ASSERT_EQ_U64(VIRTIO_BLK_BLOCK_DEVICE_BLOCK_SIZE, device.block_size);
    ASSERT_TRUE(device.flags == 0U && device.read != 0 && device.write != 0);

    g_block_count = 0U;
    ASSERT_TRUE(device.read(device.context, 7U, 2U, buffer) == 0);
    ASSERT_EQ_U64(8U, g_first_block);
    ASSERT_EQ_U64(2U, g_block_count);
    ASSERT_EQ_U64(2U, g_read_calls);

    g_block_count = 0U;
    ASSERT_TRUE(device.write(device.context, 9U, 2U, buffer) == 0);
    ASSERT_EQ_U64(10U, g_first_block);
    ASSERT_EQ_U64(2U, g_block_count);
    ASSERT_EQ_U64(2U, g_write_calls);
    return 0;
}

static int test_emmc_capacity_and_adapter(void) {
    emmc_device_t transport;
    block_device_t device;
    uint32_t raw[4];
    uint64_t capacity;
    uint8_t buffer[EMMC_BLKSZ * 2U];

    build_high_capacity_csd(raw, 8191U);
    ASSERT_TRUE(emmc_block_device_capacity_from_csd(raw, &capacity) == 0);
    ASSERT_EQ_U64(8388608U, capacity);

    build_standard_capacity_csd(raw, 1023U, 7U, 9U);
    ASSERT_TRUE(emmc_block_device_capacity_from_csd(raw, &capacity) == 0);
    ASSERT_EQ_U64(524288U, capacity);

    memset(&transport, 0, sizeof(transport));
    transport.ready = 1U;
    build_high_capacity_csd(transport.csd, 4095U);
    ASSERT_TRUE(emmc_block_device_init(&device, &transport) == 0);
    ASSERT_EQ_U64(4194304U, device.block_count);
    ASSERT_EQ_U64(EMMC_BLKSZ, device.block_size);
    ASSERT_TRUE((device.flags & BLOCK_DEVICE_FLAG_READ_ONLY) != 0U);
    ASSERT_TRUE(device.write == 0);

    g_read_calls = 0U;
    ASSERT_TRUE(device.read(device.context, 12U, 2U, buffer) == EMMC_OK);
    ASSERT_EQ_U64(12U, g_first_block);
    ASSERT_EQ_U64(2U, g_block_count);
    ASSERT_EQ_U64(1U, g_read_calls);

    transport.ready = 0U;
    ASSERT_TRUE(emmc_block_device_init(&device, &transport) != 0);
    return 0;
}

int main(void) {
    if (test_virtio_adapter() != 0 ||
        test_emmc_capacity_and_adapter() != 0) {
        return 1;
    }

    puts("storage-adapters-test: PASS");
    return 0;
}
