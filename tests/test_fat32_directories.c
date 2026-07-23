#include <stdint.h>

#include "../kernel/fat32.h"
#include "../kernel/vfs.h"

#if defined(ARMONIOS_FAT32_DIRECTORY_STANDALONE)
#include "../kernel/process.h"

process_t *process_current(void) {
    return 0;
}

process_t *process_find(uint32_t pid) {
    (void)pid;
    return 0;
}
#endif

#define DIRECTORY_TEST_SECTORS 12U

typedef struct {
    uint8_t sectors[DIRECTORY_TEST_SECTORS][FAT32_SECTOR_SIZE];
} directory_test_disk_t;

static void directory_check(int condition) {
    if (!condition) {
        __builtin_trap();
    }
}

#define DIR_CHECK(expr) directory_check((expr) != 0)
#define DIR_EQ(expected, actual) directory_check((expected) == (actual))

static void directory_le16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void directory_le32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void directory_zero(directory_test_disk_t *disk) {
    for (uint32_t sector = 0; sector < DIRECTORY_TEST_SECTORS; sector++) {
        for (uint32_t i = 0; i < FAT32_SECTOR_SIZE; i++) {
            disk->sectors[sector][i] = 0;
        }
    }
}

static void directory_name(uint8_t *entry, const char name[11]) {
    for (uint32_t i = 0; i < 11U; i++) {
        entry[i] = (uint8_t)name[i];
    }
}

static void directory_entry(uint8_t *entry, const char name[11],
                            uint8_t attributes, uint32_t cluster,
                            uint32_t size) {
    directory_name(entry, name);
    entry[11] = attributes;
    directory_le16(&entry[20], (uint16_t)(cluster >> 16));
    directory_le16(&entry[26], (uint16_t)cluster);
    directory_le32(&entry[28], size);
}

static void directory_setup(directory_test_disk_t *disk) {
    uint8_t *boot;
    uint8_t *fat;
    uint8_t *root;
    uint8_t *docs;
    uint8_t *sub;

    directory_zero(disk);
    boot = disk->sectors[0];
    fat = disk->sectors[1];
    root = disk->sectors[2];
    docs = disk->sectors[4];
    sub = disk->sectors[5];

    directory_le16(&boot[11], FAT32_SECTOR_SIZE);
    boot[13] = 1;
    directory_le16(&boot[14], 1);
    boot[16] = 1;
    directory_le16(&boot[17], 0);
    directory_le32(&boot[32], DIRECTORY_TEST_SECTORS);
    directory_le32(&boot[36], 1);
    directory_le32(&boot[44], 2);

    directory_le32(&fat[0], 0x0ffffff8U);
    directory_le32(&fat[4], 0x0fffffffU);
    directory_le32(&fat[8], 0x0fffffffU);
    directory_le32(&fat[12], 0x0fffffffU);
    directory_le32(&fat[16], 0x0fffffffU);
    directory_le32(&fat[20], 0x0fffffffU);
    directory_le32(&fat[24], 0x0fffffffU);

    directory_entry(&root[0], "DOCS       ", FAT32_ATTR_DIRECTORY, 4, 0);

    directory_entry(&docs[0], ".          ", FAT32_ATTR_DIRECTORY, 4, 0);
    directory_entry(&docs[32], "..         ", FAT32_ATTR_DIRECTORY, 2, 0);
    directory_entry(&docs[64], "SUB        ", FAT32_ATTR_DIRECTORY, 5, 0);
    directory_entry(&docs[96], "README  TXT", 0x20U, 3, 4);

    directory_entry(&sub[0], ".          ", FAT32_ATTR_DIRECTORY, 5, 0);
    directory_entry(&sub[32], "..         ", FAT32_ATTR_DIRECTORY, 4, 0);
    directory_entry(&sub[64], "NOTE    TXT", 0x20U, 6, 5);

    disk->sectors[3][0] = 'r';
    disk->sectors[3][1] = 'e';
    disk->sectors[3][2] = 'a';
    disk->sectors[3][3] = 'd';
    disk->sectors[6][0] = 'h';
    disk->sectors[6][1] = 'e';
    disk->sectors[6][2] = 'l';
    disk->sectors[6][3] = 'l';
    disk->sectors[6][4] = 'o';
}

static int directory_read(void *context, uint32_t lba, uint8_t *buffer) {
    directory_test_disk_t *disk = (directory_test_disk_t *)context;

    if (disk == 0 || buffer == 0 || lba >= DIRECTORY_TEST_SECTORS) {
        return -1;
    }
    for (uint32_t i = 0; i < FAT32_SECTOR_SIZE; i++) {
        buffer[i] = disk->sectors[lba][i];
    }
    return 0;
}

static int directory_text_equal(const uint8_t *buffer, uint64_t length,
                                const char *expected) {
    uint64_t i = 0;

    while (expected[i] != '\0') {
        if (i >= length || buffer[i] != (uint8_t)expected[i]) {
            return 0;
        }
        i++;
    }
    return i == length;
}

static void test_nested_fat32_paths(void) {
    directory_test_disk_t disk;
    fat32_fs_t fs;
    fat32_path_info_t info;
    fat32_file_t file;
    fat32_dirent_t entries[2];
    uint8_t buffer[32] = {0};
    uint64_t count = 0;

    directory_setup(&disk);
    DIR_EQ(0, fat32_mount(&fs, directory_read, &disk));

    DIR_EQ(0, fat32_lookup_path(&fs, "docs", &info));
    DIR_CHECK((info.attributes & FAT32_ATTR_DIRECTORY) != 0U);
    DIR_EQ(4U, info.first_cluster);
    DIR_EQ(0, fat32_lookup_path(&fs, "/docs/sub", &info));
    DIR_CHECK((info.attributes & FAT32_ATTR_DIRECTORY) != 0U);
    DIR_EQ(5U, info.first_cluster);
    DIR_EQ(0, fat32_open_path(&fs, "docs/sub/note.txt", &file));
    DIR_EQ(6U, file.first_cluster);
    DIR_EQ(5U, file.size);
    DIR_EQ(-1, fat32_open_path(&fs, "docs/sub", &file));
    DIR_EQ(-1, fat32_lookup_path(&fs, "docs/missing/note.txt", &info));

    DIR_EQ(0, fat32_read(&fs, &file, 0, buffer, 5, &count));
    DIR_EQ(5U, count);
    DIR_CHECK(directory_text_equal(buffer, count, "hello"));

    count = 0;
    DIR_EQ(0, fat32_list_path(&fs, "", buffer, sizeof(buffer), &count));
    DIR_CHECK(directory_text_equal(buffer, count, "DOCS/\n"));
    count = 0;
    DIR_EQ(0, fat32_list_path(&fs, "docs", buffer, sizeof(buffer), &count));
    DIR_CHECK(directory_text_equal(buffer, count, "SUB/\nREADME.TXT\n"));
    count = 0;
    DIR_EQ(0, fat32_list_path(&fs, "docs/sub", buffer, sizeof(buffer), &count));
    DIR_CHECK(directory_text_equal(buffer, count, "NOTE.TXT\n"));
    count = 0;
    DIR_EQ(0, fat32_readdir_path(&fs, "docs", 0, entries, 2, &count));
    DIR_EQ(2U, count);
    DIR_CHECK(directory_text_equal((const uint8_t *)entries[0].name,
                                   3, "SUB"));
    DIR_CHECK((entries[0].attributes & FAT32_ATTR_DIRECTORY) != 0U);
    DIR_CHECK(directory_text_equal((const uint8_t *)entries[1].name,
                                   10, "README.TXT"));
    DIR_EQ(4U, entries[1].size);
    DIR_CHECK((entries[1].attributes & FAT32_ATTR_ARCHIVE) != 0U);

    DIR_EQ(-1, fat32_list_path(&fs, "docs/readme.txt", buffer,
                               sizeof(buffer), &count));
}

static void test_nested_paths_through_vfs(void) {
    directory_test_disk_t disk;
    fat32_fs_t fs;
    vfs_stat_t stat;
    vfs_metadata_t metadata;
    vfs_dirent_t entries[2];
    uint8_t buffer[32] = {0};
    uint64_t count = 0;
    int fd;

    directory_setup(&disk);
    vfs_reset();
    fat32_vfs_reset();
    DIR_EQ(0, fat32_mount(&fs, directory_read, &disk));
    DIR_EQ(0, fat32_mount_vfs_root(&fs, "/fat"));

    DIR_EQ(0, vfs_list("/fat/docs", buffer, sizeof(buffer), &count));
    DIR_CHECK(directory_text_equal(buffer, count, "SUB/\nREADME.TXT\n"));
    count = 0;
    DIR_EQ(0, vfs_list("/fat/docs/sub", buffer, sizeof(buffer), &count));
    DIR_CHECK(directory_text_equal(buffer, count, "NOTE.TXT\n"));

    DIR_EQ(0, vfs_stat("/fat/docs/sub", &stat));
    DIR_EQ(0U, stat.size);
    DIR_EQ(0, vfs_stat("/fat/docs/sub/note.txt", &stat));
    DIR_EQ(5U, stat.size);

    DIR_EQ(0, vfs_metadata("/fat/docs/sub", &metadata));
    DIR_EQ(VFS_FILE_TYPE_DIRECTORY, metadata.type);
    DIR_EQ(0, vfs_metadata("/fat/docs/sub/note.txt", &metadata));
    DIR_EQ(VFS_FILE_TYPE_REGULAR, metadata.type);
    DIR_EQ(5U, metadata.size);
    DIR_CHECK((metadata.attributes & VFS_ATTRIBUTE_ARCHIVE) != 0U);

    count = 0;
    DIR_EQ(0, vfs_readdir("/fat/docs", 0, entries, 2, &count));
    DIR_EQ(2U, count);
    DIR_CHECK(directory_text_equal((const uint8_t *)entries[0].name,
                                   3, "SUB"));
    DIR_EQ(VFS_FILE_TYPE_DIRECTORY, entries[0].metadata.type);
    DIR_CHECK(directory_text_equal((const uint8_t *)entries[1].name,
                                   10, "README.TXT"));
    DIR_EQ(VFS_FILE_TYPE_REGULAR, entries[1].metadata.type);
    DIR_EQ(4U, entries[1].metadata.size);

    fd = vfs_open("/fat/docs/sub/../sub/note.txt");
    DIR_CHECK(fd >= 0);
    count = 0;
    DIR_EQ(0, vfs_read_fd(fd, buffer, 5, &count));
    DIR_CHECK(directory_text_equal(buffer, count, "hello"));
    DIR_EQ(0, vfs_close(fd));

    DIR_EQ(-1, vfs_open("/fat/docs/sub"));
    DIR_EQ(-1, vfs_open_flags("/fat/docs/sub/new.txt",
                               VFS_O_RDWR | VFS_O_CREAT));
    DIR_EQ(-1, vfs_unlink("/fat/docs/sub/note.txt"));
    DIR_EQ(-1, vfs_rename("/fat/docs/sub/note.txt",
                           "/fat/docs/sub/other.txt"));
}

__attribute__((constructor))
static void fat32_directory_constructor(void) {
    test_nested_fat32_paths();
    test_nested_paths_through_vfs();
}

#if defined(ARMONIOS_FAT32_DIRECTORY_STANDALONE)
int main(void) {
    return 0;
}
#endif
