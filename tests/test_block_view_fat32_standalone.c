#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "kernel/fat32.h"
#include "storage/block_device.h"

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
    uint64_t last_lba;
    uint32_t read_count;
    uint32_t write_count;
    uint32_t flush_count;
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

static int fake_read(void *context, uint64_t first_block,
                     uint32_t block_count, void *buffer) {
    fake_disk_t *disk = (fake_disk_t *)context;
    uint8_t *bytes = (uint8_t *)buffer;

    if (disk == 0 || buffer == 0 || first_block >= 16U ||
        block_count > 16U - first_block) {
        return -1;
    }

    for (uint32_t i = 0; i < block_count; i++) {
        memcpy(bytes + i * FAT32_SECTOR_SIZE,
               disk->sectors[first_block + i], FAT32_SECTOR_SIZE);
    }
    disk->last_lba = first_block + block_count - 1U;
    disk->read_count += block_count;
    return 0;
}

static int fake_write(void *context, uint64_t first_block,
                      uint32_t block_count, const void *buffer) {
    fake_disk_t *disk = (fake_disk_t *)context;
    const uint8_t *bytes = (const uint8_t *)buffer;

    if (disk == 0 || buffer == 0 || first_block >= 16U ||
        block_count > 16U - first_block) {
        return -1;
    }

    for (uint32_t i = 0; i < block_count; i++) {
        memcpy(disk->sectors[first_block + i],
               bytes + i * FAT32_SECTOR_SIZE, FAT32_SECTOR_SIZE);
    }
    disk->last_lba = first_block + block_count - 1U;
    disk->write_count += block_count;
    return 0;
}

static int fake_flush(void *context) {
    fake_disk_t *disk = (fake_disk_t *)context;

    if (disk == 0) {
        return -1;
    }
    disk->flush_count++;
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
    block_device_t parent;
    block_device_t invalid;
    block_device_view_t fat_view;
    block_device_view_t writable_fat_view;
    block_device_view_t writable_view;
    block_device_view_t nested_view;
    const block_device_t *fat_device;
    fat32_fs_t fs;
    fat32_fs_t writable_fs;
    uint8_t list[16];
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint64_t written = 99U;
    uint32_t reads_before;

    memset(&disk, 0, sizeof(disk));
    memset(&fs, 0, sizeof(fs));
    memset(&writable_fs, 0, sizeof(writable_fs));
    build_fat32_boot_sector(disk.sectors[8]);
    disk.sectors[10][0] = 0x00U;

    ASSERT_TRUE(block_device_init(&invalid, fake_read, 0, 0, &disk,
                                  16U, FAT32_SECTOR_SIZE, 0U) != 0);
    ASSERT_EQ_U32(0U, block_device_init(&parent, fake_read, fake_write,
                                        fake_flush, &disk, 16U,
                                        FAT32_SECTOR_SIZE, 0U));
    ASSERT_TRUE(!block_device_is_read_only(&parent));
    ASSERT_EQ_U32(0U, block_device_read(&parent, 16U, 0U, 0));
    ASSERT_TRUE(block_device_read(&parent, 15U, 2U, sector) != 0);

    ASSERT_EQ_U32(0U, block_device_view_init(
                          &fat_view, &parent, 8U, 8U,
                          BLOCK_DEVICE_FLAG_READ_ONLY));
    fat_device = block_device_view_device(&fat_view);
    ASSERT_TRUE(fat_device != 0);
    ASSERT_TRUE(block_device_is_read_only(fat_device));
    ASSERT_EQ_U32(8U, fat_device->block_count);
    ASSERT_EQ_U32(FAT32_SECTOR_SIZE, fat_device->block_size);

    put_le32(&disk.sectors[8][32], 9U);
    ASSERT_TRUE(fat32_mount_device(&fs, fat_device) != 0);
    ASSERT_TRUE(fs.mounted == 0U);
    put_le32(&disk.sectors[8][32], 8U);

    ASSERT_EQ_U32(0U, fat32_mount_device(&fs, fat_device));
    ASSERT_TRUE(fs.mounted != 0U);
    ASSERT_TRUE(fs.write_sector == 0);
    ASSERT_EQ_U32(8U, disk.last_lba);

    ASSERT_EQ_U32(0U, fat32_list_root(&fs, list, sizeof(list), &written));
    ASSERT_EQ_U32(0U, written);
    ASSERT_EQ_U32(10U, disk.last_lba);

    reads_before = disk.read_count;
    ASSERT_TRUE(block_device_read(fat_device, 8U, 1U, sector) != 0);
    ASSERT_EQ_U32(reads_before, disk.read_count);
    ASSERT_TRUE(block_device_write(fat_device, 0U, 1U, sector) != 0);
    ASSERT_EQ_U32(0U, fat32_flush(&fs));
    ASSERT_EQ_U32(1U, disk.flush_count);

    ASSERT_EQ_U32(0U, block_device_view_init(&writable_fat_view, &parent,
                                             8U, 8U, 0U));
    ASSERT_EQ_U32(0U, fat32_mount_device(
                          &writable_fs,
                          block_device_view_device(&writable_fat_view)));
    ASSERT_TRUE(writable_fs.write_sector != 0);
    ASSERT_EQ_U32(0U, fat32_flush(&writable_fs));
    ASSERT_EQ_U32(2U, disk.flush_count);

    ASSERT_EQ_U32(0U, block_device_view_init(&writable_view, &parent,
                                             0U, 4U, 0U));
    memset(sector, 0xa5, sizeof(sector));
    ASSERT_EQ_U32(0U, block_device_write(
                          block_device_view_device(&writable_view),
                          1U, 1U, sector));
    ASSERT_EQ_U32(1U, disk.last_lba);
    ASSERT_EQ_U32(1U, disk.write_count);
    ASSERT_EQ_U32(0xa5U, disk.sectors[1][0]);

    ASSERT_EQ_U32(0U, block_device_view_init(
                          &nested_view,
                          block_device_view_device(&fat_view), 2U, 2U, 0U));
    ASSERT_TRUE(block_device_is_read_only(
        block_device_view_device(&nested_view)));
    ASSERT_EQ_U32(0U, block_device_read(
                          block_device_view_device(&nested_view),
                          0U, 1U, sector));
    ASSERT_EQ_U32(10U, disk.last_lba);

    ASSERT_TRUE(block_device_view_init(&fat_view, &parent, 15U, 2U, 0U) != 0);
    ASSERT_TRUE(block_device_view_init(&fat_view, &parent, UINT64_MAX,
                                       1U, 0U) != 0);

    puts("block-device-view-fat32-test: PASS");
    return 0;
}
