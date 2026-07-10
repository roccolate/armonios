#include <stdint.h>

#include "../kernel/fat32.h"
#include "../kernel/vfs.h"

#define TEST_FAT32_SECTORS 8U

#define CHECK_TRUE(expr) do { if (!(expr)) { __builtin_trap(); } } while (0)
#define CHECK_EQ(expected, actual) CHECK_TRUE((expected) == (actual))

typedef struct {
    uint8_t data[8];
    uint64_t size;
} test_vfs_file_t;

typedef struct {
    uint8_t sectors[TEST_FAT32_SECTORS][FAT32_SECTOR_SIZE];
    uint32_t sector_count;
} test_fat32_disk_t;

static int test_vfs_file_read(void *context, uint64_t offset, uint8_t *buffer,
                              uint64_t capacity, uint64_t *bytes_read) {
    test_vfs_file_t *file = (test_vfs_file_t *)context;
    uint64_t count;

    if (bytes_read != 0) {
        *bytes_read = 0;
    }
    if (file == 0 || buffer == 0 || bytes_read == 0 || offset > file->size) {
        return -1;
    }

    count = capacity;
    if (count > file->size - offset) {
        count = file->size - offset;
    }
    for (uint64_t i = 0; i < count; i++) {
        buffer[i] = file->data[offset + i];
    }
    *bytes_read = count;
    return 0;
}

static int test_vfs_file_write(void *context, uint64_t offset,
                               const uint8_t *buffer, uint64_t size,
                               uint64_t *bytes_written) {
    test_vfs_file_t *file = (test_vfs_file_t *)context;

    if (bytes_written != 0) {
        *bytes_written = 0;
    }
    if (file == 0 || buffer == 0 || bytes_written == 0 ||
        offset > file->size || offset + size > sizeof(file->data)) {
        return -1;
    }

    for (uint64_t i = 0; i < size; i++) {
        file->data[offset + i] = buffer[i];
    }
    if (offset + size > file->size) {
        file->size = offset + size;
    }
    *bytes_written = size;
    return 0;
}

static int test_vfs_file_stat(void *context, vfs_stat_t *stat) {
    test_vfs_file_t *file = (test_vfs_file_t *)context;

    if (file == 0 || stat == 0) {
        return -1;
    }
    stat->size = file->size;
    return 0;
}

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

static int test_bad_read_sector(void *context, uint32_t lba,
                                uint8_t *buffer) {
    (void)context;
    (void)lba;
    (void)buffer;
    return -1;
}

static void test_clear_default_fat32(void) {
    fat32_fs_t bad_fs;
    (void)fat32_mount(&bad_fs, test_bad_read_sector, 0);
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

static void test_vfs_unmount_static_invalidates_descriptors(void) {
    test_vfs_file_t file = {
        .data = { 'o', 'l', 'd' },
        .size = 3,
    };
    test_vfs_file_t replacement = {
        .data = { 'n', 'e', 'w' },
        .size = 3,
    };
    vfs_node_t node = {
        .path = "/fat/old.txt",
        .size = 3,
        .read = test_vfs_file_read,
        .write = test_vfs_file_write,
        .stat = test_vfs_file_stat,
        .context = &file,
    };
    vfs_node_t replacement_node = {
        .path = "/fat/old.txt",
        .size = 3,
        .read = test_vfs_file_read,
        .write = test_vfs_file_write,
        .stat = test_vfs_file_stat,
        .context = &replacement,
    };
    uint8_t buffer[4] = { 0 };
    uint64_t count = 99;
    int fd;

    vfs_reset();
    CHECK_EQ(0, vfs_mount_static(&node, 1));
    fd = vfs_open_flags("/fat/old.txt", VFS_O_RDONLY);
    CHECK_TRUE(fd >= 0);

    CHECK_EQ(0, vfs_unmount_static("/fat/old.txt"));
    CHECK_TRUE(vfs_find("/fat/old.txt") == 0);
    CHECK_EQ(-1, vfs_read_fd(fd, buffer, sizeof(buffer), &count));
    CHECK_EQ(0U, count);

    CHECK_EQ(0, vfs_mount_static(&replacement_node, 1));
    fd = vfs_open_flags("/fat/old.txt", VFS_O_RDONLY);
    CHECK_TRUE(fd >= 0);
    CHECK_EQ(0, vfs_read_fd(fd, buffer, sizeof(buffer), &count));
    CHECK_EQ(3U, count);
    CHECK_EQ('n', buffer[0]);
    CHECK_EQ(0, vfs_close(fd));
}

static void test_vfs_fat32_rename_delete_invalidates_dynamic_nodes(void) {
    test_fat32_disk_t disk;
    fat32_fs_t fs;
    uint8_t buffer[6] = { 0 };
    uint64_t count = 99;
    int fd;

    test_setup_fat32_disk(&disk);
    vfs_reset();
    fat32_vfs_reset();

    CHECK_EQ(0, fat32_mount(&fs, test_disk_read_sector, &disk));
    fat32_set_write_sector(&fs, test_disk_write_sector);

    fd = vfs_open_flags("/fat/hello.txt", VFS_O_RDONLY);
    CHECK_TRUE(fd >= 0);
    CHECK_TRUE(vfs_find("/fat/hello.txt") != 0);

    CHECK_EQ(0, vfs_rename("/fat/hello.txt", "/fat/bye.txt"));
    CHECK_TRUE(vfs_find("/fat/hello.txt") == 0);
    CHECK_EQ(-1, vfs_read_fd(fd, buffer, sizeof(buffer), &count));
    CHECK_EQ(0U, count);
    CHECK_EQ(-1, vfs_open_flags("/fat/hello.txt", VFS_O_RDONLY));

    fd = vfs_open_flags("/fat/bye.txt", VFS_O_RDONLY);
    CHECK_TRUE(fd >= 0);
    CHECK_TRUE(vfs_find("/fat/bye.txt") != 0);
    CHECK_EQ(0, vfs_close(fd));

    CHECK_EQ(0, vfs_unlink("/fat/bye.txt"));
    CHECK_TRUE(vfs_find("/fat/bye.txt") == 0);
    CHECK_EQ(-1, vfs_open_flags("/fat/bye.txt", VFS_O_RDONLY));

    vfs_reset();
    fat32_vfs_reset();
    test_clear_default_fat32();
}

__attribute__((constructor))
static void test_vfs_fat32_invalidation_constructor(void) {
    test_vfs_unmount_static_invalidates_descriptors();
    test_vfs_fat32_rename_delete_invalidates_dynamic_nodes();
}
