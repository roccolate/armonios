#include <stdint.h>

#include "kernel/fat32.h"

#define TEST_SECTORS 8U
#define EOC 0x0ffffff8U
#define BAD 0x0ffffff7U

typedef struct {
    uint8_t sectors[TEST_SECTORS][FAT32_SECTOR_SIZE];
    uint32_t reads;
    uint32_t writes;
    uint32_t highest_lba;
    uint32_t out_of_range;
} test_disk_t;

static void put16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static int disk_read(void *context, uint32_t lba, uint8_t *buffer) {
    test_disk_t *disk = (test_disk_t *)context;

    disk->reads++;
    if (lba >= TEST_SECTORS) {
        disk->out_of_range++;
        return -1;
    }
    if (lba > disk->highest_lba) {
        disk->highest_lba = lba;
    }
    for (uint32_t i = 0; i < FAT32_SECTOR_SIZE; i++) {
        buffer[i] = disk->sectors[lba][i];
    }
    return 0;
}

static int disk_write(void *context, uint32_t lba,
                      const uint8_t *buffer) {
    test_disk_t *disk = (test_disk_t *)context;

    disk->writes++;
    if (lba >= TEST_SECTORS) {
        disk->out_of_range++;
        return -1;
    }
    if (lba > disk->highest_lba) {
        disk->highest_lba = lba;
    }
    for (uint32_t i = 0; i < FAT32_SECTOR_SIZE; i++) {
        disk->sectors[lba][i] = buffer[i];
    }
    return 0;
}

static void clear_disk(test_disk_t *disk) {
    for (uint32_t sector = 0; sector < TEST_SECTORS; sector++) {
        for (uint32_t byte = 0; byte < FAT32_SECTOR_SIZE; byte++) {
            disk->sectors[sector][byte] = 0;
        }
    }
    disk->reads = 0;
    disk->writes = 0;
    disk->highest_lba = 0;
    disk->out_of_range = 0;
}

static void set_name(uint8_t *entry, const char name[11]) {
    for (uint32_t i = 0; i < 11U; i++) {
        entry[i] = (uint8_t)name[i];
    }
}

static void setup_valid_disk(test_disk_t *disk) {
    uint8_t *boot;
    uint8_t *fat;
    uint8_t *root;

    clear_disk(disk);
    boot = disk->sectors[0];
    fat = disk->sectors[1];
    root = disk->sectors[2];

    put16(&boot[11], FAT32_SECTOR_SIZE);
    boot[13] = 1;
    put16(&boot[14], 1);
    boot[16] = 1;
    put16(&boot[17], 0);
    put32(&boot[32], TEST_SECTORS);
    put32(&boot[36], 1);
    put32(&boot[44], 2);

    put32(&fat[0], EOC);
    put32(&fat[4], 0x0fffffffU);
    put32(&fat[8], EOC);
    put32(&fat[12], 4);
    put32(&fat[16], EOC);

    set_name(root, "HELLO   TXT");
    root[11] = 0x20U;
    put16(&root[26], 3);
    put32(&root[28], FAT32_SECTOR_SIZE + 5U);

    disk->sectors[3][FAT32_SECTOR_SIZE - 6U] = 'H';
    disk->sectors[3][FAT32_SECTOR_SIZE - 5U] = 'e';
    disk->sectors[3][FAT32_SECTOR_SIZE - 4U] = 'l';
    disk->sectors[3][FAT32_SECTOR_SIZE - 3U] = 'l';
    disk->sectors[3][FAT32_SECTOR_SIZE - 2U] = 'o';
    disk->sectors[3][FAT32_SECTOR_SIZE - 1U] = ' ';
    disk->sectors[4][0] = 'W';
    disk->sectors[4][1] = 'o';
    disk->sectors[4][2] = 'r';
    disk->sectors[4][3] = 'l';
    disk->sectors[4][4] = 'd';
}

static int expect(int condition, int line) {
    return condition ? 0 : line;
}

#define CHECK(condition) do { \
    int failed_line = expect((condition), __LINE__); \
    if (failed_line != 0) return failed_line; \
} while (0)

static int test_valid_geometry_and_read(void) {
    test_disk_t disk;
    fat32_fs_t fs;
    fat32_file_t file;
    uint8_t output[16] = { 0 };
    uint64_t count = 0;

    setup_valid_disk(&disk);
    CHECK(fat32_mount(&fs, disk_read, &disk) == 0);
    CHECK(fs.cluster_count == 6U);
    CHECK(fat32_open_root(&fs, "hello.txt", &file) == 0);
    CHECK(file.capacity == FAT32_SECTOR_SIZE * 2U);
    CHECK(fat32_read(&fs, &file, FAT32_SECTOR_SIZE - 6U,
                     output, sizeof(output), &count) == 0);
    CHECK(count == 11U);
    CHECK(output[0] == 'H' && output[5] == ' ' && output[10] == 'd');
    CHECK(disk.out_of_range == 0);
    return 0;
}

static int test_self_cycle_is_rejected(void) {
    test_disk_t disk;
    fat32_fs_t fs;
    fat32_file_t file;

    setup_valid_disk(&disk);
    put32(&disk.sectors[1][12], 3);
    CHECK(fat32_mount(&fs, disk_read, &disk) == 0);
    CHECK(fat32_open_root(&fs, "HELLO.TXT", &file) == -1);
    CHECK(disk.reads <= fs.cluster_count + 3U);
    CHECK(disk.out_of_range == 0);
    return 0;
}

static int test_two_cluster_cycle_is_rejected(void) {
    test_disk_t disk;
    fat32_fs_t fs;
    fat32_file_t file;

    setup_valid_disk(&disk);
    put32(&disk.sectors[1][16], 3);
    CHECK(fat32_mount(&fs, disk_read, &disk) == 0);
    CHECK(fat32_open_root(&fs, "HELLO.TXT", &file) == -1);
    CHECK(disk.out_of_range == 0);
    return 0;
}

static int test_invalid_chain_values_are_rejected(void) {
    static const uint32_t invalid[] = { 0U, 1U, BAD, 0x0ffffff2U, 8U };
    test_disk_t disk;
    fat32_fs_t fs;
    fat32_file_t file;

    for (uint32_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        setup_valid_disk(&disk);
        put32(&disk.sectors[1][12], invalid[i]);
        CHECK(fat32_mount(&fs, disk_read, &disk) == 0);
        CHECK(fat32_open_root(&fs, "HELLO.TXT", &file) == -1);
        CHECK(disk.out_of_range == 0);
    }
    return 0;
}

static int test_root_cycle_is_bounded(void) {
    test_disk_t disk;
    fat32_fs_t fs;
    uint8_t output[32];
    uint64_t written = 0;

    setup_valid_disk(&disk);
    for (uint32_t i = 0; i < FAT32_SECTOR_SIZE; i += 32U) {
        disk.sectors[2][i] = 0xe5U;
    }
    put32(&disk.sectors[1][8], 2);
    CHECK(fat32_mount(&fs, disk_read, &disk) == 0);
    CHECK(fat32_list_root(&fs, output, sizeof(output), &written) == -1);
    CHECK(disk.reads <= fs.cluster_count * 2U + 2U);
    CHECK(disk.out_of_range == 0);
    return 0;
}

static int test_mount_rejects_invalid_geometry(void) {
    test_disk_t disk;
    fat32_fs_t fs;

    setup_valid_disk(&disk);
    put32(&disk.sectors[0][44], 8);
    CHECK(fat32_mount(&fs, disk_read, &disk) == -1);
    CHECK(fs.mounted == 0);

    setup_valid_disk(&disk);
    disk.sectors[0][16] = 255U;
    put32(&disk.sectors[0][36], UINT32_MAX);
    CHECK(fat32_mount(&fs, disk_read, &disk) == -1);
    CHECK(disk.reads == 1U);
    CHECK(disk.out_of_range == 0);

    setup_valid_disk(&disk);
    put32(&disk.sectors[0][32], 1000U);
    CHECK(fat32_mount(&fs, disk_read, &disk) == -1);
    CHECK(disk.out_of_range == 0);
    return 0;
}

static int test_size_larger_than_chain_is_rejected(void) {
    test_disk_t disk;
    fat32_fs_t fs;
    fat32_file_t file;

    setup_valid_disk(&disk);
    put32(&disk.sectors[2][28], FAT32_SECTOR_SIZE * 2U + 1U);
    CHECK(fat32_mount(&fs, disk_read, &disk) == 0);
    CHECK(fat32_open_root(&fs, "HELLO.TXT", &file) == -1);
    CHECK(disk.out_of_range == 0);
    return 0;
}

static int test_corrupt_delete_does_not_mutate_media(void) {
    test_disk_t disk;
    fat32_fs_t fs;
    uint32_t writes_before;

    setup_valid_disk(&disk);
    put32(&disk.sectors[1][12], 3);
    CHECK(fat32_mount(&fs, disk_read, &disk) == 0);
    fat32_set_write_sector(&fs, disk_write);
    writes_before = disk.writes;
    CHECK(fat32_delete(&fs, "HELLO.TXT") == -1);
    CHECK(disk.writes == writes_before);
    CHECK(disk.sectors[2][0] == 'H');
    CHECK(disk.sectors[1][12] == 3U);
    CHECK(disk.out_of_range == 0);
    return 0;
}

int main(void) {
    int result;

    result = test_valid_geometry_and_read();
    if (result != 0) return result;
    result = test_self_cycle_is_rejected();
    if (result != 0) return result;
    result = test_two_cluster_cycle_is_rejected();
    if (result != 0) return result;
    result = test_invalid_chain_values_are_rejected();
    if (result != 0) return result;
    result = test_root_cycle_is_bounded();
    if (result != 0) return result;
    result = test_mount_rejects_invalid_geometry();
    if (result != 0) return result;
    result = test_size_larger_than_chain_is_rejected();
    if (result != 0) return result;
    result = test_corrupt_delete_does_not_mutate_media();
    if (result != 0) return result;
    return 0;
}
