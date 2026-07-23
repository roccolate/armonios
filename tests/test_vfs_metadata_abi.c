#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "include/armonios/abi/syscall_numbers.h"
#include "include/armonios/abi/version.h"
#include "include/armonios/abi/vfs.h"
#include "kernel/vfs.h"

static const char *listing_for(const char *path) {
    if (strcmp(path, "/fat/docs") == 0) {
        return "README.TXT\nSUB/\n";
    }
    if (strcmp(path, "/fat/docs/SUB") == 0) {
        return "NOTE.TXT\n";
    }
    return 0;
}

int vfs_stat(const char *path, vfs_stat_t *stat) {
    if (path == 0 || stat == 0) {
        return -1;
    }
    if (strcmp(path, "/fat/docs") == 0 ||
        strcmp(path, "/fat/docs/SUB") == 0) {
        stat->size = 0;
        return 0;
    }
    if (strcmp(path, "/fat/docs/README.TXT") == 0) {
        stat->size = 12;
        return 0;
    }
    if (strcmp(path, "/fat/docs/SUB/NOTE.TXT") == 0) {
        stat->size = 4;
        return 0;
    }
    return -1;
}

int vfs_list_at(const char *path, uint64_t offset, uint8_t *buffer,
                uint64_t capacity, uint64_t *bytes_written) {
    const char *listing = listing_for(path);
    uint64_t length;

    if (bytes_written != 0) {
        *bytes_written = 0;
    }
    if (listing == 0 || buffer == 0 || bytes_written == 0) {
        return -1;
    }
    length = strlen(listing);
    if (offset >= length || capacity == 0) {
        return 0;
    }
    buffer[0] = (uint8_t)listing[offset];
    *bytes_written = 1;
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

    if (!require(ARMONIOS_ABI_VERSION == 0x00010001U, "ABI version") ||
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
        !require(stat.size == 12, "regular size")) {
        return 1;
    }

    if (!require(vfs_readdir_v2("/fat/docs", 0, entries, 2, &count) == 0,
                 "readdir first page") ||
        !require(count == 2, "readdir count") ||
        !require(strcmp(entries[0].name, "README.TXT") == 0,
                 "first name") ||
        !require(entries[0].type == ARM_FILE_TYPE_REGULAR &&
                     entries[0].size == 12,
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
