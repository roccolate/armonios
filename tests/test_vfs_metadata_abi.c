#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "include/armonios/abi/syscall_numbers.h"
#include "include/armonios/abi/version.h"
#include "include/armonios/abi/vfs.h"
#include "kernel/vfs.h"

int vfs_metadata(const char *path, vfs_metadata_t *metadata) {
    if (path == 0 || metadata == 0) {
        return -1;
    }

    metadata->size = 0;
    metadata->attributes = 0;
    if (strcmp(path, "/fat/docs") == 0 ||
        strcmp(path, "/fat/docs/SUB") == 0) {
        metadata->type = VFS_FILE_TYPE_DIRECTORY;
        return 0;
    }
    if (strcmp(path, "/fat/docs/README.TXT") == 0) {
        metadata->size = 12;
        metadata->type = VFS_FILE_TYPE_REGULAR;
        metadata->attributes = VFS_ATTRIBUTE_ARCHIVE;
        return 0;
    }
    if (strcmp(path, "/fat/docs/SUB/NOTE.TXT") == 0) {
        metadata->size = 4;
        metadata->type = VFS_FILE_TYPE_REGULAR;
        metadata->attributes = VFS_ATTRIBUTE_READ_ONLY;
        return 0;
    }
    return -1;
}

static void mock_dirent(vfs_dirent_t *entry, const char *name,
                        uint64_t size, uint32_t type,
                        uint32_t attributes) {
    uint32_t i = 0;

    while (i + 1U < VFS_NAME_MAX && name[i] != '\0') {
        entry->name[i] = name[i];
        i++;
    }
    entry->name[i] = '\0';
    entry->metadata.size = size;
    entry->metadata.type = type;
    entry->metadata.attributes = attributes;
}

int vfs_readdir(const char *path, uint64_t start_index,
                vfs_dirent_t *entries, uint64_t max_entries,
                uint64_t *entries_written) {
    uint64_t output = 0;

    if (entries_written != 0) {
        *entries_written = 0;
    }
    if (path == 0 || entries == 0 || entries_written == 0 ||
        max_entries == 0 || strcmp(path, "/fat/docs") != 0) {
        return -1;
    }

    if (start_index == 0 && output < max_entries) {
        mock_dirent(&entries[output++], "README.TXT", 12,
                    VFS_FILE_TYPE_REGULAR, VFS_ATTRIBUTE_ARCHIVE);
    }
    if (start_index <= 1 && output < max_entries) {
        mock_dirent(&entries[output++], "SUB", 0,
                    VFS_FILE_TYPE_DIRECTORY, 0);
    }
    *entries_written = output;
    return 0;
}

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void) {
    arm_stat_v2_t stat;
    arm_dirent_v2_t entries[2];
    uint64_t count = 0;

    if (!require(ARMONIOS_ABI_VERSION == 0x00010000U, "ABI version") ||
        !require(SYS_STAT == 45ULL && SYS_READDIR == 46ULL,
                 "legacy syscall numbers") ||
        !require(SYS_STAT_V2 == 49ULL && SYS_READDIR_V2 == 50ULL,
                 "structured syscall numbers") ||
        !require(sizeof(arm_stat_t) == 8U, "legacy stat layout") ||
        !require(sizeof(arm_stat_v2_t) == 32U, "stat v2 layout") ||
        !require(sizeof(arm_dirent_v2_t) == 96U, "dirent v2 layout")) {
        return 1;
    }

    if (!require(vfs_stat_v2("/fat/docs", &stat) == 0,
                 "directory stat") ||
        !require(stat.version == ARM_VFS_METADATA_VERSION,
                 "stat version") ||
        !require(stat.struct_size == sizeof(stat), "stat struct size") ||
        !require(stat.type == ARM_FILE_TYPE_DIRECTORY, "directory type") ||
        !require(stat.size == 0 && stat.attributes == 0 && stat.reserved == 0,
                 "directory metadata")) {
        return 1;
    }

    if (!require(vfs_stat_v2("/fat/docs/README.TXT", &stat) == 0,
                 "file stat") ||
        !require(stat.type == ARM_FILE_TYPE_REGULAR, "regular type") ||
        !require(stat.size == 12, "regular size") ||
        !require(stat.attributes == ARM_FILE_ATTRIBUTE_ARCHIVE,
                 "archive attribute")) {
        return 1;
    }

    if (!require(vfs_readdir_v2("/fat/docs", 0, entries, 2, &count) == 0,
                 "readdir first page") ||
        !require(count == 2, "readdir count") ||
        !require(strcmp(entries[0].name, "README.TXT") == 0,
                 "first name") ||
        !require(entries[0].type == ARM_FILE_TYPE_REGULAR &&
                     entries[0].size == 12 &&
                     entries[0].attributes == ARM_FILE_ATTRIBUTE_ARCHIVE,
                 "first metadata") ||
        !require(strcmp(entries[1].name, "SUB") == 0,
                 "directory name without slash") ||
        !require(entries[1].type == ARM_FILE_TYPE_DIRECTORY,
                 "second type")) {
        return 1;
    }

    count = 0;
    if (!require(vfs_readdir_v2("/fat/docs", 1, entries, 1, &count) == 0,
                 "readdir indexed page") ||
        !require(count == 1 && strcmp(entries[0].name, "SUB") == 0,
                 "indexed result")) {
        return 1;
    }

    puts("PASS: structured VFS metadata ABI");
    return 0;
}
