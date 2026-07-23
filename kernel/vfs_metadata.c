#include "kernel/vfs.h"

#include <stdint.h>

static uint32_t vfs_public_type(uint32_t type) {
    if (type == VFS_FILE_TYPE_REGULAR) {
        return ARM_FILE_TYPE_REGULAR;
    }
    if (type == VFS_FILE_TYPE_DIRECTORY) {
        return ARM_FILE_TYPE_DIRECTORY;
    }
    return ARM_FILE_TYPE_UNKNOWN;
}

static uint32_t vfs_public_attributes(uint32_t attributes) {
    uint32_t value = 0;

    if ((attributes & VFS_ATTRIBUTE_READ_ONLY) != 0U) {
        value |= ARM_FILE_ATTR_READ_ONLY;
    }
    if ((attributes & VFS_ATTRIBUTE_HIDDEN) != 0U) {
        value |= ARM_FILE_ATTR_HIDDEN;
    }
    if ((attributes & VFS_ATTRIBUTE_SYSTEM) != 0U) {
        value |= ARM_FILE_ATTR_SYSTEM;
    }
    if ((attributes & VFS_ATTRIBUTE_ARCHIVE) != 0U) {
        value |= ARM_FILE_ATTR_ARCHIVE;
    }
    return value;
}

static void vfs_public_clear_stat(arm_stat_v2_t *stat) {
    stat->version = ARM_VFS_METADATA_VERSION;
    stat->struct_size = sizeof(*stat);
    stat->size = 0;
    stat->type = ARM_FILE_TYPE_UNKNOWN;
    stat->attributes = 0;
    stat->reserved = 0;
}

static void vfs_public_clear_dirent(arm_dirent_v2_t *entry) {
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

static int vfs_public_copy_name(char destination[ARM_DIRENT_NAME_MAX],
                                const char source[VFS_NAME_MAX]) {
    uint32_t i = 0;

    if (destination == 0 || source == 0 || source[0] == '\0') {
        return -1;
    }
    while (i < VFS_NAME_MAX && source[i] != '\0') {
        if (i + 1U >= ARM_DIRENT_NAME_MAX) {
            return -1;
        }
        destination[i] = source[i];
        i++;
    }
    if (i == VFS_NAME_MAX) {
        return -1;
    }
    destination[i] = '\0';
    return 0;
}

int vfs_stat_v2(const char *path, arm_stat_v2_t *stat) {
    vfs_metadata_t metadata;

    if (path == 0 || stat == 0 || vfs_metadata(path, &metadata) != 0) {
        return -1;
    }

    vfs_public_clear_stat(stat);
    stat->size = metadata.size;
    stat->type = vfs_public_type(metadata.type);
    stat->attributes = vfs_public_attributes(metadata.attributes);
    return 0;
}

int vfs_readdir_v2(const char *path, uint64_t start_index,
                   arm_dirent_v2_t *entries, uint64_t max_entries,
                   uint64_t *entries_written) {
    vfs_dirent_t native[VFS_READDIR_MAX_ENTRIES];
    uint64_t written = 0;

    if (entries_written != 0) {
        *entries_written = 0;
    }
    if (path == 0 || entries == 0 || entries_written == 0 ||
        max_entries == 0 || max_entries > VFS_READDIR_MAX_ENTRIES ||
        vfs_readdir(path, start_index, native, max_entries, &written) != 0 ||
        written > max_entries) {
        return -1;
    }

    for (uint64_t i = 0; i < written; i++) {
        vfs_public_clear_dirent(&entries[i]);
        if (vfs_public_copy_name(entries[i].name, native[i].name) != 0) {
            return -1;
        }
        entries[i].size = native[i].metadata.size;
        entries[i].type = vfs_public_type(native[i].metadata.type);
        entries[i].attributes =
            vfs_public_attributes(native[i].metadata.attributes);
    }

    *entries_written = written;
    return 0;
}
