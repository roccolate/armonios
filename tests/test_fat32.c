#include <stdint.h>

#include "unity/unity.h"
#include "../kernel/fat32.h"
#include "../kernel/vfs.h"

#define TEST_FAT32_SECTORS 8U

typedef struct {
    uint8_t sectors[TEST_FAT32_SECTORS][FAT32_SECTOR_SIZE];
    uint32_t sector_count;
} test_fat32_disk_t;

static void test_write_le16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void test_write_le32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void test_disk_reset(test_fat32_disk_t *disk) {
    disk->sector_count = TEST_FAT32_SECTORS;
    for (uint32_t s = 0; s < TEST_FAT32_SECTORS; s++) {
        for (uint32_t i = 0; i < FAT32_SECTOR_SIZE; i++) {
            disk->sectors[s][i] = 0;
        }
    }
}

static int test_disk_read_sector(void *context, uint32_t lba,
                                 uint8_t *buffer) {
    test_fat32_disk_t *disk = (test_fat32_disk_t *)context;

    if (disk == 0 || buffer == 0 || lba >= disk->sector_count) {
        return -1;
    }

    for (uint32_t i = 0; i < FAT32_SECTOR_SIZE; i++) {
        buffer[i] = disk->sectors[lba][i];
    }

    return 0;
}

static int test_disk_write_sector(void *context, uint32_t lba,
                                  const uint8_t *buffer) {
    test_fat32_disk_t *disk = (test_fat32_disk_t *)context;

    if (disk == 0 || buffer == 0 || lba >= disk->sector_count) {
        return -1;
    }

    for (uint32_t i = 0; i < FAT32_SECTOR_SIZE; i++) {
        disk->sectors[lba][i] = buffer[i];
    }

    return 0;
}

static void test_dir_name(uint8_t entry[32], const char name[11]) {
    for (uint32_t i = 0; i < 11U; i++) {
        entry[i] = (uint8_t)name[i];
    }
}

static void test_setup_fat32_disk(test_fat32_disk_t *disk) {
    uint8_t *boot;
    uint8_t *fat;
    uint8_t *root;

    test_disk_reset(disk);
    boot = disk->sectors[0];
    fat = disk->sectors[1];
    root = disk->sectors[2];

    test_write_le16(&boot[11], FAT32_SECTOR_SIZE);
    boot[13] = 1;
    test_write_le16(&boot[14], 1);
    boot[16] = 1;
    test_write_le16(&boot[17], 0);
    test_write_le32(&boot[32], TEST_FAT32_SECTORS);
    test_write_le32(&boot[36], 1);
    test_write_le32(&boot[44], 2);
    boot[510] = 0x55;
    boot[511] = 0xaa;

    test_write_le32(&fat[0], 0x0ffffff8U);
    test_write_le32(&fat[4], 0x0fffffffU);
    test_write_le32(&fat[8], 0x0fffffffU);
    test_write_le32(&fat[12], 4);
    test_write_le32(&fat[16], 0x0fffffffU);

    test_dir_name(root, "HELLO   TXT");
    root[11] = 0x20;
    test_write_le16(&root[20], 0);
    test_write_le16(&root[26], 3);
    test_write_le32(&root[28], FAT32_SECTOR_SIZE + 5U);

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

void test_fat32_mount_reads_geometry(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;

    test_setup_fat32_disk(&disk);

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_mount(
                                  &fs, test_disk_read_sector, &disk));
    TEST_ASSERT_EQUAL_UINT64(FAT32_SECTOR_SIZE, fs.bytes_per_sector);
    TEST_ASSERT_EQUAL_UINT64(1, fs.sectors_per_cluster);
    TEST_ASSERT_EQUAL_UINT64(1, fs.fat_start_lba);
    TEST_ASSERT_EQUAL_UINT64(2, fs.data_start_lba);
    TEST_ASSERT_EQUAL_UINT64(2, fs.root_cluster);
}

void test_fat32_open_root_finds_8_3_file(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;
    fat32_file_t file;

    test_setup_fat32_disk(&disk);

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_mount(
                                  &fs, test_disk_read_sector, &disk));
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_open_root(&fs, "hello.txt",
                                                          &file));
    TEST_ASSERT_EQUAL_UINT64(3, file.first_cluster);
    TEST_ASSERT_EQUAL_UINT64(2, file.dir_lba);
    TEST_ASSERT_EQUAL_UINT64(0, file.dir_offset);
    TEST_ASSERT_EQUAL_UINT64(2U * FAT32_SECTOR_SIZE, file.capacity);
    TEST_ASSERT_EQUAL_UINT64(FAT32_SECTOR_SIZE + 5U, file.size);
}

void test_fat32_read_file_across_cluster_chain(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;
    fat32_file_t file;
    uint8_t buffer[16] = { 0 };
    uint64_t bytes_read = 99;

    test_setup_fat32_disk(&disk);

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_mount(
                                  &fs, test_disk_read_sector, &disk));
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_open_root(&fs, "HELLO.TXT",
                                                          &file));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)fat32_read(
                                 &fs, &file, FAT32_SECTOR_SIZE - 6U,
                                 buffer, sizeof(buffer), &bytes_read));
    TEST_ASSERT_EQUAL_UINT64(11, bytes_read);
    TEST_ASSERT_EQUAL_UINT64('H', buffer[0]);
    TEST_ASSERT_EQUAL_UINT64('e', buffer[1]);
    TEST_ASSERT_EQUAL_UINT64('l', buffer[2]);
    TEST_ASSERT_EQUAL_UINT64('l', buffer[3]);
    TEST_ASSERT_EQUAL_UINT64('o', buffer[4]);
    TEST_ASSERT_EQUAL_UINT64(' ', buffer[5]);
    TEST_ASSERT_EQUAL_UINT64('W', buffer[6]);
    TEST_ASSERT_EQUAL_UINT64('o', buffer[7]);
    TEST_ASSERT_EQUAL_UINT64('r', buffer[8]);
    TEST_ASSERT_EQUAL_UINT64('l', buffer[9]);
    TEST_ASSERT_EQUAL_UINT64('d', buffer[10]);
    TEST_ASSERT_EQUAL_UINT64(0, buffer[11]);
}

void test_fat32_read_clamps_and_honors_offset(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;
    fat32_file_t file;
    uint8_t buffer[4] = { 0 };
    uint64_t bytes_read = 99;

    test_setup_fat32_disk(&disk);

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_mount(
                                  &fs, test_disk_read_sector, &disk));
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_open_root(&fs, "/HELLO.TXT",
                                                          &file));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)fat32_read(&fs, &file,
                                                  FAT32_SECTOR_SIZE, buffer,
                                                  sizeof(buffer),
                                                  &bytes_read));
    TEST_ASSERT_EQUAL_UINT64(sizeof(buffer), bytes_read);
    TEST_ASSERT_EQUAL_UINT64('W', buffer[0]);
    TEST_ASSERT_EQUAL_UINT64('o', buffer[1]);
    TEST_ASSERT_EQUAL_UINT64('r', buffer[2]);
    TEST_ASSERT_EQUAL_UINT64('l', buffer[3]);
}

void test_fat32_rejects_invalid_boot_sector_and_missing_files(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;
    fat32_file_t file;

    test_setup_fat32_disk(&disk);
    test_write_le16(&disk.sectors[0][11], 1024);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)fat32_mount(&fs, test_disk_read_sector,
                                                   &disk));

    test_setup_fat32_disk(&disk);
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_mount(
                                  &fs, test_disk_read_sector, &disk));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)fat32_open_root(&fs, "MISSING.TXT",
                                                       &file));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)fat32_open_root(&fs, "too-long.txt",
                                                       &file));
}

void test_fat32_mount_vfs_file_reads_through_vfs(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;
    uint8_t buffer[11] = { 0 };
    uint64_t bytes_read = 99;
    vfs_stat_t stat;

    test_setup_fat32_disk(&disk);
    vfs_reset();
    fat32_vfs_reset();

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_mount(
                                  &fs, test_disk_read_sector, &disk));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)fat32_mount_vfs_file(
                                 &fs, "/fat/hello.txt", "HELLO.TXT"));
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)vfs_stat("/fat/hello.txt", &stat));
    TEST_ASSERT_EQUAL_UINT64(FAT32_SECTOR_SIZE + 5U, stat.size);
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)vfs_read(
                                 "/fat/hello.txt", FAT32_SECTOR_SIZE - 6U,
                                 buffer, sizeof(buffer), &bytes_read));
    TEST_ASSERT_EQUAL_UINT64(sizeof(buffer), bytes_read);
    TEST_ASSERT_EQUAL_UINT64('H', buffer[0]);
    TEST_ASSERT_EQUAL_UINT64('e', buffer[1]);
    TEST_ASSERT_EQUAL_UINT64('l', buffer[2]);
    TEST_ASSERT_EQUAL_UINT64('l', buffer[3]);
    TEST_ASSERT_EQUAL_UINT64('o', buffer[4]);
    TEST_ASSERT_EQUAL_UINT64(' ', buffer[5]);
    TEST_ASSERT_EQUAL_UINT64('W', buffer[6]);
    TEST_ASSERT_EQUAL_UINT64('o', buffer[7]);
    TEST_ASSERT_EQUAL_UINT64('r', buffer[8]);
    TEST_ASSERT_EQUAL_UINT64('l', buffer[9]);
    TEST_ASSERT_EQUAL_UINT64('d', buffer[10]);
}

void test_fat32_list_root_returns_short_names(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;
    uint8_t buffer[16] = { 0 };
    uint64_t bytes_written = 99;
    const uint8_t expected[] = "HELLO.TXT\n";

    test_setup_fat32_disk(&disk);

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_mount(
                                  &fs, test_disk_read_sector, &disk));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)fat32_list_root(
                                 &fs, buffer, sizeof(buffer),
                                 &bytes_written));
    TEST_ASSERT_EQUAL_UINT64(sizeof(expected) - 1U, bytes_written);
    for (uint64_t i = 0; i < bytes_written; i++) {
        TEST_ASSERT_EQUAL_UINT64(expected[i], buffer[i]);
    }
}

void test_fat32_mount_vfs_root_lists_through_vfs(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;
    uint8_t buffer[16] = { 0 };
    uint64_t bytes_written = 99;
    const uint8_t expected[] = "HELLO.TXT\n";

    test_setup_fat32_disk(&disk);
    vfs_reset();
    fat32_vfs_reset();

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_mount(
                                  &fs, test_disk_read_sector, &disk));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)fat32_mount_vfs_root(&fs, "/fat"));
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)vfs_list("/fat", buffer,
                                                  sizeof(buffer),
                                                  &bytes_written));
    TEST_ASSERT_EQUAL_UINT64(sizeof(expected) - 1U, bytes_written);
    for (uint64_t i = 0; i < bytes_written; i++) {
        TEST_ASSERT_EQUAL_UINT64(expected[i], buffer[i]);
    }
}

void test_fat32_write_overwrites_and_updates_size(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;
    fat32_file_t file;
    uint8_t input[] = { 'O', 'K', '\n' };
    uint8_t output[8] = { 0 };
    uint64_t count = 99;

    test_setup_fat32_disk(&disk);

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_mount(
                                  &fs, test_disk_read_sector, &disk));
    fat32_set_write_sector(&fs, test_disk_write_sector);
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_open_root(&fs, "HELLO.TXT",
                                                          &file));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)fat32_write(&fs, &file, 0, input,
                                                   sizeof(input), &count));
    TEST_ASSERT_EQUAL_UINT64(sizeof(input), count);
    TEST_ASSERT_EQUAL_UINT64(sizeof(input), file.size);
    TEST_ASSERT_EQUAL_UINT64(sizeof(input),
                             (uint64_t)(disk.sectors[2][28] |
                                        ((uint32_t)disk.sectors[2][29] << 8) |
                                        ((uint32_t)disk.sectors[2][30] << 16) |
                                        ((uint32_t)disk.sectors[2][31] << 24)));

    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)fat32_read(&fs, &file, 0, output,
                                                  sizeof(output), &count));
    TEST_ASSERT_EQUAL_UINT64(sizeof(input), count);
    TEST_ASSERT_EQUAL_UINT64('O', output[0]);
    TEST_ASSERT_EQUAL_UINT64('K', output[1]);
    TEST_ASSERT_EQUAL_UINT64('\n', output[2]);
}

void test_fat32_write_grows_within_chain_and_rejects_capacity_overflow(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;
    fat32_file_t file;
    uint8_t input[] = { 'Z', 'Z', 'Z', 'Z' };
    uint8_t output[4] = { 0 };
    uint64_t count = 99;

    test_setup_fat32_disk(&disk);

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_mount(
                                  &fs, test_disk_read_sector, &disk));
    fat32_set_write_sector(&fs, test_disk_write_sector);
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_open_root(&fs, "HELLO.TXT",
                                                          &file));

    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)fat32_write(
                                 &fs, &file, FAT32_SECTOR_SIZE + 5U, input,
                                 sizeof(input), &count));
    TEST_ASSERT_EQUAL_UINT64(sizeof(input), count);
    TEST_ASSERT_EQUAL_UINT64(FAT32_SECTOR_SIZE + 9U, file.size);

    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)fat32_read(
                                 &fs, &file, FAT32_SECTOR_SIZE + 5U, output,
                                 sizeof(output), &count));
    TEST_ASSERT_EQUAL_UINT64(sizeof(output), count);
    TEST_ASSERT_EQUAL_UINT64('Z', output[0]);
    TEST_ASSERT_EQUAL_UINT64('Z', output[3]);

    count = 99;
    TEST_ASSERT_EQUAL_UINT64((uint64_t)-1,
                             (uint64_t)fat32_write(
                                 &fs, &file, file.capacity - 1U, input,
                                 sizeof(input), &count));
    TEST_ASSERT_EQUAL_UINT64(0, count);
}

void test_fat32_mount_vfs_file_writes_through_vfs(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;
    uint8_t input[] = { 'v', 'f', 's' };
    uint8_t output[4] = { 0 };
    uint64_t count = 99;
    int fd;
    vfs_stat_t stat;

    test_setup_fat32_disk(&disk);
    vfs_reset();
    fat32_vfs_reset();

    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)fat32_mount(
                                  &fs, test_disk_read_sector, &disk));
    fat32_set_write_sector(&fs, test_disk_write_sector);
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)fat32_mount_vfs_file(
                                 &fs, "/fat/hello.txt", "HELLO.TXT"));

    fd = vfs_open_flags("/fat/hello.txt", VFS_O_RDWR);
    TEST_ASSERT_TRUE(fd >= 0);
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)vfs_write_fd(fd, input,
                                                    sizeof(input), &count));
    TEST_ASSERT_EQUAL_UINT64(sizeof(input), count);
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)vfs_seek(fd, 0));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)vfs_read_fd(fd, output,
                                                   sizeof(output), &count));
    TEST_ASSERT_EQUAL_UINT64(sizeof(input), count);
    TEST_ASSERT_EQUAL_UINT64('v', output[0]);
    TEST_ASSERT_EQUAL_UINT64('f', output[1]);
    TEST_ASSERT_EQUAL_UINT64('s', output[2]);
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)vfs_close(fd));
    TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)vfs_stat("/fat/hello.txt", &stat));
    TEST_ASSERT_EQUAL_UINT64(sizeof(input), stat.size);
}
