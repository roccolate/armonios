#include "kernel/vfs.h"

#include <stdint.h>

#include "kernel/kstring.h"

#define VFS_METADATA_SCAN_LIMIT 4096U

static void vfs_metadata_clear_stat(arm_stat_v2_t *stat) {
    stat->version = ARM_VFS_METADATA_VERSION;
    stat->struct_size = sizeof(*stat);
    stat->size = 0;
    stat->type = ARM_FILE_TYPE_UNKNOWN;
    stat->attributes = 0;
    stat->reserved = 0;
}

int vfs_stat_v2(const char *path, arm_stat_v2_t *stat) {
    vfs_stat_t legacy;
    uint8_t probe = 0;
    uint64_t listed = 0;

    if (path == 0 || stat == 0 || vfs_stat(path, &legacy) != 0) {
        return -1;
    }

    vfs_metadata_clear_stat(stat);
    stat->size = legacy.size;
    stat->type = vfs_list_at(path, 0, &probe, 1, &listed) == 0
                     ? ARM_FILE_TYPE_DIRECTORY
                     : ARM_FILE_TYPE_REGULAR;
    return 0;
}

static int vfs_metadata_copy_name(char destination[ARM_DIRENT_NAME_MAX],
                                  const char *source, uint32_t length) {
    if (destination == 0 || source == 0 || length == 0 ||
        length >= ARM_DIRENT_NAME_MAX) {
        return -1;
    }

    for (uint32_t i = 0; i < length; i++) {
        destination[i] = source[i];
    }
    destination[length] = '\0';
    for (uint32_t i = length + 1U; i < ARM_DIRENT_NAME_MAX; i++) {
        destination[i] = '\0';
    }
    return 0;
}

static int vfs_metadata_child_path(const char *directory, const char *name,
                                   char child[VFS_MAX_PATH]) {
    uint32_t out = 0;

    if (directory == 0 || name == 0 || child == 0 || directory[0] != '/') {
        return -1;
    }

    while (directory[out] != '\0') {
        if (out + 1U >= VFS_MAX_PATH) {
            return -1;
        }
        child[out] = directory[out];
        out++;
    }
    if (out > 1U) {
        if (out + 1U >= VFS_MAX_PATH) {
            return -1;
        }
        child[out++] = '/';
    }
    for (uint32_t i = 0; name[i] != '\0'; i++) {
        if (out + 1U >= VFS_MAX_PATH) {
            return -1;
        }
        child[out++] = name[i];
    }
    child[out] = '\0';
    return 0;
}

static void vfs_metadata_init_dirent(arm_dirent_v2_t *entry) {
    entry->version = ARM_VFS_METADATA_VERSION;
    entry->struct_size = sizeof(*entry);
    entry->size = 0;
    entry->type = ARM_FILE_TYPE_UNKNOWN;
    entry->attributes = 0;
    entry->reserved = 0;
    for (uint32_t i = 0; i < ARM_DIRENT_NAME_MAX; i++) {
        entry->name[i] = '\0';
    }
}

int vfs_readdir_v2(const char *path, uint64_t start_index,
                   arm_dirent_v2_t *entries, uint64_t max_entries,
                   uint64_t *entries_written) {
    char line[ARM_DIRENT_NAME_MAX + 1U];
    uint64_t byte_offset = 0;
    uint64_t logical_index = 0;
    uint64_t output = 0;
    uint32_t line_length = 0;

    if (entries_written != 0) {
        *entries_written = 0;
    }
    if (path == 0 || entries == 0 || entries_written == 0 ||
        max_entries == 0) {
        return -1;
    }

    for (uint64_t scanned = 0; scanned < VFS_METADATA_SCAN_LIMIT; scanned++) {
        uint8_t value = 0;
        uint64_t count = 0;

        if (vfs_list_at(path, byte_offset, &value, 1, &count) != 0) {
            return -1;
        }
        if (count == 0) {
            if (line_length != 0) {
                return -1;
            }
            *entries_written = output;
            return 0;
        }
        byte_offset++;

        if (value != '\n') {
            if (line_length >= ARM_DIRENT_NAME_MAX) {
                return -1;
            }
            line[line_length++] = (char)value;
            continue;
        }

        if (line_length == 0) {
            continue;
        }

        if (logical_index++ < start_index) {
            line_length = 0;
            continue;
        }

        arm_dirent_v2_t *entry = &entries[output];
        uint32_t name_length = line_length;
        char child[VFS_MAX_PATH];
        arm_stat_v2_t stat;

        vfs_metadata_init_dirent(entry);
        if (line[name_length - 1U] == '/') {
            name_length--;
            entry->type = ARM_FILE_TYPE_DIRECTORY;
        }
        if (vfs_metadata_copy_name(entry->name, line, name_length) != 0) {
            return -1;
        }

        if (vfs_metadata_child_path(path, entry->name, child) == 0 &&
            vfs_stat_v2(child, &stat) == 0) {
            entry->size = stat.size;
            entry->type = stat.type;
            entry->attributes = stat.attributes;
        } else if (entry->type == ARM_FILE_TYPE_UNKNOWN) {
            entry->type = ARM_FILE_TYPE_REGULAR;
        }

        output++;
        line_length = 0;
        if (output == max_entries) {
            *entries_written = output;
            return 0;
        }
    }

    return -1;
}
