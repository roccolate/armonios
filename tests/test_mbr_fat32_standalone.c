#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "storage/mbr.h"

#define TEST_BLOCKS 64U

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "assert failed: %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

#define ASSERT_EQ_U32(expected, actual) do { \
    uint32_t e_ = (uint32_t)(expected); \
    uint32_t a_ = (uint32_t)(actual); \
    if (e_ != a_) { \
        fprintf(stderr, \
                "assert failed: %s:%d: expected 0x%08x got 0x%08x\n", \
                __FILE__, __LINE__, e_, a_); \
        return 1; \
    } \
} while (0)

typedef struct {
    uint8_t sectors[TEST_BLOCKS][MBR_SECTOR_SIZE];
    uint64_t last_block;
    uint32_t reads;
} fake_disk_t;

static int fake_read(void *context, uint64_t first_block,
                     uint32_t block_count, void *buffer) {
    fake_disk_t *disk = (fake_disk_t *)context;

    if (disk == 0 || buffer == 0 || block_count == 0U ||
        first_block >= TEST_BLOCKS || block_count > TEST_BLOCKS - first_block) {
        return -1;
    }

    memcpy(buffer, disk->sectors[first_block],
           (size_t)block_count * MBR_SECTOR_SIZE);
    disk->last_block = first_block + block_count - 1U;
    disk->reads += block_count;
    return 0;
}

static void put_le32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
    p[2] = (uint8_t)(value >> 16U);
    p[3] = (uint8_t)(value >> 24U);
}

static void set_partition(uint8_t sector[MBR_SECTOR_SIZE], uint32_t index,
                          uint8_t boot, uint8_t type, uint32_t start_lba,
                          uint32_t sector_count) {
    uint8_t *entry = &sector[446U + index * 16U];

    entry[0] = boot;
    entry[4] = type;
    put_le32(&entry[8], start_lba);
    put_le32(&entry[12], sector_count);
}

static int test_finds_first_valid_fat32_partition(void) {
    uint8_t sector[MBR_SECTOR_SIZE];
    mbr_partition_t partition;

    memset(sector, 0, sizeof(sector));
    sector[510] = 0x55U;
    sector[511] = 0xaaU;
    set_partition(sector, 0U, 0U, 0x83U, 2048U, 4096U);
    set_partition(sector, 1U, 0x80U, 0x0cU, 8192U, 65536U);

    ASSERT_EQ_U32(0U, mbr_find_fat32_partition(sector, &partition));
    ASSERT_EQ_U32(0x0cU, partition.type);
    ASSERT_EQ_U32(8192U, partition.start_lba);
    ASSERT_EQ_U32(65536U, partition.sector_count);
    return 0;
}

static int test_accepts_hidden_fat32_and_skips_invalid_entries(void) {
    uint8_t sector[MBR_SECTOR_SIZE];
    mbr_partition_t partition;

    memset(sector, 0, sizeof(sector));
    sector[510] = 0x55U;
    sector[511] = 0xaaU;
    set_partition(sector, 0U, 0x7fU, 0x0bU, 100U, 1000U);
    set_partition(sector, 1U, 0U, 0x0cU, UINT32_MAX, 2U);
    set_partition(sector, 2U, 0U, 0x1cU, 4096U, 32768U);

    ASSERT_EQ_U32(0U, mbr_find_fat32_partition(sector, &partition));
    ASSERT_EQ_U32(0x1cU, partition.type);
    ASSERT_EQ_U32(4096U, partition.start_lba);
    ASSERT_EQ_U32(32768U, partition.sector_count);
    return 0;
}

static int test_rejects_missing_signature_and_empty_layout(void) {
    uint8_t sector[MBR_SECTOR_SIZE];
    mbr_partition_t partition;

    memset(sector, 0, sizeof(sector));
    set_partition(sector, 0U, 0U, 0x0bU, 2048U, 4096U);
    ASSERT_TRUE(mbr_find_fat32_partition(sector, &partition) != 0);

    sector[510] = 0x55U;
    sector[511] = 0xaaU;
    set_partition(sector, 0U, 0U, 0x0bU, 0U, 4096U);
    ASSERT_TRUE(mbr_find_fat32_partition(sector, &partition) != 0);
    return 0;
}

static int test_opens_bounded_partition_view(void) {
    fake_disk_t disk;
    block_device_t parent;
    block_device_view_t view;
    mbr_partition_t partition;
    uint8_t sector[MBR_SECTOR_SIZE];
    const block_device_t *child;

    memset(&disk, 0, sizeof(disk));
    disk.sectors[0][510] = 0x55U;
    disk.sectors[0][511] = 0xaaU;
    set_partition(disk.sectors[0], 0U, 0U, 0x0cU, 8U, 16U);
    disk.sectors[8][0] = 0x5aU;

    ASSERT_EQ_U32(0U, block_device_init(
                          &parent, fake_read, 0, 0, &disk, TEST_BLOCKS,
                          MBR_SECTOR_SIZE, BLOCK_DEVICE_FLAG_READ_ONLY));
    ASSERT_EQ_U32(0U, mbr_open_fat32_partition(&parent, &view, &partition));
    ASSERT_EQ_U32(8U, partition.start_lba);
    ASSERT_EQ_U32(16U, partition.sector_count);

    child = block_device_view_device(&view);
    ASSERT_TRUE(child != 0 && block_device_is_read_only(child));
    ASSERT_EQ_U32(16U, child->block_count);
    ASSERT_EQ_U32(0U, block_device_read(child, 0U, 1U, sector));
    ASSERT_EQ_U32(8U, disk.last_block);
    ASSERT_EQ_U32(0x5aU, sector[0]);
    ASSERT_TRUE(block_device_read(child, 16U, 1U, sector) != 0);

    set_partition(disk.sectors[0], 0U, 0U, 0x0cU, 60U, 16U);
    ASSERT_TRUE(mbr_open_fat32_partition(&parent, &view, &partition) != 0);
    return 0;
}

int main(void) {
    if (test_finds_first_valid_fat32_partition() != 0 ||
        test_accepts_hidden_fat32_and_skips_invalid_entries() != 0 ||
        test_rejects_missing_signature_and_empty_layout() != 0 ||
        test_opens_bounded_partition_view() != 0) {
        return 1;
    }
    puts("mbr-fat32-test: PASS");
    return 0;
}
