#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "kernel/fat32.h"
#include "storage/block_view.h"

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
    uint8_t sectors[16][FAT32_SECTOR_SIZE];
    uint32_t last_lba;
    uint32_t read_count;
} fake_disk_t;

static void put_le16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
}

static void put_le32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8U);
    p[2] = (uint8_t)(value >> 16U);
    p[3] = (uint8_t)(value >> 24U);
}

static int fake_read(void *context, uint32_t lba, uint8_t *buffer) {
    fake_disk_t *disk = (fake_disk_t *)context;

    if (lba >= 16U) {
        return -1;
    }
    memcpy(buffer, disk->sectors[lba], FAT32_SECTOR_SIZE);
    disk->last_lba = lba;
    disk->read_count++;
    return 0;
}

static void build_fat32_boot_sector(uint8_t *sector) {
    memset(sector, 0, FAT32_SECTOR_SIZE);
    put_le16(&sector[11], FAT32_SECTOR_SIZE);
    sector[13] = 1U;
    put_le16(&sector[14], 1U);
    sector[16] = 1U;
    put_le16(&sector[17], 0U);
    put_le32(&sector[32], 8U);
    put_le32(&sector[36], 1U);
    put_le32(&sector[44], 2U);
    sector[510] = 0x55U;
    sector[511] = 0xaaU;
}

int main(void) {
    fake_disk_t disk;
    block_view_t view;
    fat32_fs_t fs;
    uint8_t list[16];
    uint64_t written = 99U;
    uint32_t reads_before;

    memset(&disk, 0, sizeof(disk));
    memset(&fs, 0, sizeof(fs));
    build_fat32_boot_sector(disk.sectors[8]);
    disk.sectors[10][0] = 0x00U;

    ASSERT_EQ_U32(0U, block_view_init(&view, fake_read, 0, &disk,
                                      8U, 8U));
    ASSERT_EQ_U32(0U, fat32_mount(&fs, block_view_read_sector, &view));
    ASSERT_TRUE(fs.mounted != 0U);
    ASSERT_EQ_U32(8U, disk.last_lba);

    ASSERT_EQ_U32(0U, fat32_list_root(&fs, list, sizeof(list), &written));
    ASSERT_EQ_U32(0U, written);
    ASSERT_EQ_U32(10U, disk.last_lba);

    reads_before = disk.read_count;
    ASSERT_TRUE(block_view_read_sector(&view, 8U, list) != 0);
    ASSERT_EQ_U32(reads_before, disk.read_count);
    ASSERT_TRUE(block_view_write_sector(&view, 0U, list) != 0);
    ASSERT_TRUE(block_view_init(&view, fake_read, 0, &disk,
                                UINT32_MAX, 2U) != 0);

    puts("block-view-fat32-test: PASS");
    return 0;
}
