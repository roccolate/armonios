#include "kernel/fat32.h"

#include <stdint.h>

#include "include/armonios/abi/errors.h"
#include "kernel/vfs.h"

#define FAT32_MAX_VFS_FILES 8U
#define FAT32_VFS_ROOT_PATH "/fat"

typedef struct {
    fat32_fs_t *fs;
    fat32_file_t file;
} fat32_vfs_file_t;

typedef struct {
    fat32_fs_t *fs;
} fat32_vfs_mount_t;

static fat32_vfs_file_t g_fat32_vfs_files[FAT32_MAX_VFS_FILES];
static vfs_node_t g_fat32_vfs_nodes[FAT32_MAX_VFS_FILES];
static uint32_t g_fat32_vfs_count;
static fat32_vfs_mount_t g_fat32_vfs_mount;
static uint8_t g_fat32_vfs_registered;

_Static_assert(FAT32_MAX_VFS_FILES <= VFS_MAX_NODES,
               "FAT32 VFS files must fit the VFS static node table");

static void clear_vfs_slot(uint32_t index) {
    if (index >= FAT32_MAX_VFS_FILES) {
        return;
    }

    g_fat32_vfs_files[index].fs = 0;
    g_fat32_vfs_files[index].file.first_cluster = 0;
    g_fat32_vfs_files[index].file.dir_lba = 0;
    g_fat32_vfs_files[index].file.dir_offset = 0;
    g_fat32_vfs_files[index].file.capacity = 0;
    g_fat32_vfs_files[index].file.size = 0;
    g_fat32_vfs_nodes[index].path = 0;
    g_fat32_vfs_nodes[index].size = 0;
    g_fat32_vfs_nodes[index].read = 0;
    g_fat32_vfs_nodes[index].write = 0;
    g_fat32_vfs_nodes[index].stat = 0;
    g_fat32_vfs_nodes[index].context = 0;
}

static fat32_fs_t *fat32_vfs_fs(void *context) {
    fat32_vfs_mount_t *mount = (fat32_vfs_mount_t *)context;
    fat32_fs_t *fs = mount != 0 ? mount->fs : 0;

    return fs != 0 && fs->mounted != 0 ? fs : 0;
}

static int fat32_vfs_relative_path(const char *path,
                                   char out[VFS_MAX_PATH]) {
    const char *suffix;
    uint32_t i = 0;

    if (path == 0 || out == 0) {
        return -1;
    }
    if (path[0] == '/' && path[1] == 'f' && path[2] == 'a' &&
        path[3] == 't' && path[4] == '\0') {
        out[0] = '\0';
        return 0;
    }

    suffix = vfs_strip_prefix(path, FAT32_VFS_ROOT_PATH);
    if (suffix == 0 || suffix[0] != '/' || suffix[1] == '\0') {
        return -1;
    }
    suffix++;
    while (suffix[i] != '\0') {
        if (i + 1U >= VFS_MAX_PATH) {
            return -1;
        }
        out[i] = suffix[i];
        i++;
    }
    out[i] = '\0';
    return 0;
}

static int fat32_vfs_root_name(const char *path, char *out,
                               uint64_t out_size) {
    char relative[VFS_MAX_PATH];
    uint64_t i = 0;

    if (out == 0 || out_size == 0 ||
        fat32_vfs_relative_path(path, relative) != 0 || relative[0] == '\0') {
        return -1;
    }

    while (relative[i] != '\0') {
        if (relative[i] == '/' || i + 1U >= out_size) {
            return -1;
        }
        out[i] = relative[i];
        i++;
    }
    out[i] = '\0';
    return 0;
}

static int fat32_vfs_read(void *context, uint64_t offset, uint8_t *buffer,
                          uint64_t capacity, uint64_t *bytes_read) {
    fat32_vfs_file_t *mounted = (fat32_vfs_file_t *)context;

    if (mounted == 0 || mounted->fs == 0) {
        return -1;
    }
    return fat32_read(mounted->fs, &mounted->file, offset, buffer, capacity,
                      bytes_read);
}

static int fat32_vfs_write(void *context, uint64_t offset,
                           const uint8_t *buffer, uint64_t size,
                           uint64_t *bytes_written) {
    fat32_vfs_file_t *mounted = (fat32_vfs_file_t *)context;

    if (mounted == 0 || mounted->fs == 0) {
        return -1;
    }
    return fat32_write(mounted->fs, &mounted->file, offset, buffer, size,
                       bytes_written);
}

static int fat32_vfs_stat(void *context, vfs_stat_t *stat) {
    fat32_vfs_file_t *mounted = (fat32_vfs_file_t *)context;

    if (mounted == 0 || mounted->fs == 0 || stat == 0) {
        return -1;
    }
    stat->size = mounted->file.size;
    return 0;
}

static int fat32_vfs_stat_path(void *context, const char *path,
                               vfs_stat_t *stat) {
    char relative[VFS_MAX_PATH];
    fat32_path_info_t info;
    fat32_fs_t *fs = fat32_vfs_fs(context);

    if (fs == 0 || stat == 0 ||
        fat32_vfs_relative_path(path, relative) != 0 ||
        fat32_lookup_path(fs, relative, &info) != 0) {
        return -1;
    }
    stat->size = info.size;
    return 0;
}


static uint32_t fat32_vfs_attributes(uint8_t attributes) {
    uint32_t value = 0;

    if ((attributes & FAT32_ATTR_READ_ONLY) != 0U) {
        value |= VFS_ATTRIBUTE_READ_ONLY;
    }
    if ((attributes & FAT32_ATTR_HIDDEN) != 0U) {
        value |= VFS_ATTRIBUTE_HIDDEN;
    }
    if ((attributes & FAT32_ATTR_SYSTEM) != 0U) {
        value |= VFS_ATTRIBUTE_SYSTEM;
    }
    if ((attributes & FAT32_ATTR_ARCHIVE) != 0U) {
        value |= VFS_ATTRIBUTE_ARCHIVE;
    }
    return value;
}

static int fat32_vfs_metadata_path(void *context, const char *path,
                                   vfs_metadata_t *metadata) {
    char relative[VFS_MAX_PATH];
    fat32_path_info_t info;
    fat32_fs_t *fs = fat32_vfs_fs(context);

    if (fs == 0 || metadata == 0 ||
        fat32_vfs_relative_path(path, relative) != 0 ||
        fat32_lookup_path(fs, relative, &info) != 0) {
        return -1;
    }
    metadata->size = info.size;
    metadata->type = (info.attributes & FAT32_ATTR_DIRECTORY) != 0U
                         ? VFS_FILE_TYPE_DIRECTORY
                         : VFS_FILE_TYPE_REGULAR;
    metadata->attributes = fat32_vfs_attributes(info.attributes);
    return 0;
}

static int fat32_vfs_readdir_path(void *context, const char *path,
                                  uint64_t start_index,
                                  vfs_dirent_t *entries,
                                  uint64_t max_entries,
                                  uint64_t *entries_written) {
    char relative[VFS_MAX_PATH];
    fat32_dirent_t native[VFS_READDIR_MAX_ENTRIES];
    fat32_fs_t *fs = fat32_vfs_fs(context);
    uint64_t written = 0;

    if (entries_written != 0) {
        *entries_written = 0;
    }
    if (fs == 0 || entries == 0 || entries_written == 0 ||
        max_entries == 0 || max_entries > VFS_READDIR_MAX_ENTRIES ||
        fat32_vfs_relative_path(path, relative) != 0 ||
        fat32_readdir_path(fs, relative, start_index, native, max_entries,
                           &written) != 0 ||
        written > max_entries) {
        return -1;
    }

    for (uint64_t i = 0; i < written; i++) {
        uint32_t j = 0;

        for (; j + 1U < VFS_NAME_MAX && native[i].name[j] != '\0'; j++) {
            entries[i].name[j] = native[i].name[j];
        }
        if (native[i].name[j] != '\0') {
            return -1;
        }
        entries[i].name[j] = '\0';
        for (j++; j < VFS_NAME_MAX; j++) {
            entries[i].name[j] = '\0';
        }
        entries[i].metadata.size = native[i].size;
        entries[i].metadata.type =
            (native[i].attributes & FAT32_ATTR_DIRECTORY) != 0U
                ? VFS_FILE_TYPE_DIRECTORY
                : VFS_FILE_TYPE_REGULAR;
        entries[i].metadata.attributes =
            fat32_vfs_attributes(native[i].attributes);
    }

    *entries_written = written;
    return 0;
}

static int fat32_vfs_list_path(void *context, const char *path,
                               uint64_t offset, uint8_t *buffer,
                               uint64_t capacity, uint64_t *bytes_written) {
    char relative[VFS_MAX_PATH];
    fat32_fs_t *fs = fat32_vfs_fs(context);

    if (fs == 0 || fat32_vfs_relative_path(path, relative) != 0) {
        return -1;
    }
    return fat32_list_path_at(fs, relative, offset, buffer, capacity,
                              bytes_written);
}


static int fat32_vfs_fsinfo(void *context, const char *path,
                             vfs_fsinfo_t *info) {
    char relative[VFS_MAX_PATH];
    fat32_fs_t *fs = fat32_vfs_fs(context);
    static const char name[] = "fat32";

    if (fs == 0 || info == 0 ||
        fat32_vfs_relative_path(path, relative) != 0 ||
        fs->total_sectors == 0) {
        return ARMONIOS_ERR_INVAL;
    }

    info->total_bytes = (uint64_t)fs->total_sectors * FAT32_SECTOR_SIZE;
    info->free_bytes = 0;
    info->block_size = FAT32_SECTOR_SIZE;
    info->max_name_length = FAT32_SHORT_NAME_MAX - 1U;
    info->max_path_length = VFS_MAX_PATH - 1U;
    info->flags = VFS_FS_FLAG_DIRECTORIES;
    if (fs->write_sector == 0) {
        info->flags |= VFS_FS_FLAG_READ_ONLY;
    }
    if (fs->flush_supported != 0U) {
        info->flags |= VFS_FS_FLAG_FLUSH;
    }
    for (uint32_t i = 0; i < VFS_FILESYSTEM_NAME_MAX; i++) {
        info->filesystem[i] = '\0';
    }
    for (uint32_t i = 0; name[i] != '\0'; i++) {
        info->filesystem[i] = name[i];
    }
    return 0;
}

static int fat32_vfs_list_root(void *context, uint64_t offset,
                               uint8_t *buffer, uint64_t capacity,
                               uint64_t *bytes_written) {
    fat32_fs_t *fs = fat32_vfs_fs(context);

    if (fs == 0) {
        return -1;
    }
    return fat32_list_path_at(fs, "", offset, buffer, capacity,
                              bytes_written);
}

static int fat32_vfs_open_path(void *context, const char *path,
                               uint32_t flags) {
    char relative[VFS_MAX_PATH];
    char root_name[VFS_MAX_PATH];
    fat32_file_t file;
    fat32_fs_t *fs = fat32_vfs_fs(context);

    if (fs == 0 || fat32_vfs_relative_path(path, relative) != 0 ||
        relative[0] == '\0') {
        return -1;
    }

    if (fat32_open_path(fs, relative, &file) != 0) {
        if ((flags & VFS_O_CREAT) == 0 ||
            fat32_vfs_root_name(path, root_name, sizeof(root_name)) != 0 ||
            fat32_create(fs, root_name, &file) != 0) {
            return -1;
        }
    }

    return fat32_mount_vfs_file(fs, path, relative);
}

static int fat32_vfs_unlink_path(void *context, const char *path) {
    char name[VFS_MAX_PATH];
    fat32_fs_t *fs = fat32_vfs_fs(context);

    if (fs == 0 || fat32_vfs_root_name(path, name, sizeof(name)) != 0) {
        return -1;
    }
    return fat32_delete(fs, name);
}

static int fat32_vfs_rename_path(void *context, const char *old_path,
                                 const char *new_path) {
    char old_name[VFS_MAX_PATH];
    char new_name[VFS_MAX_PATH];
    fat32_fs_t *fs = fat32_vfs_fs(context);

    if (fs == 0 ||
        fat32_vfs_root_name(old_path, old_name, sizeof(old_name)) != 0 ||
        fat32_vfs_root_name(new_path, new_name, sizeof(new_name)) != 0) {
        return -1;
    }
    return fat32_rename(fs, old_name, new_name);
}

static const vfs_mount_ops_t g_fat32_vfs_ops = {
    .open = fat32_vfs_open_path,
    .list = fat32_vfs_list_root,
    .stat_path = fat32_vfs_stat_path,
    .list_path = fat32_vfs_list_path,
    .metadata_path = fat32_vfs_metadata_path,
    .readdir_path = fat32_vfs_readdir_path,
    .fsinfo = fat32_vfs_fsinfo,
    .unlink = fat32_vfs_unlink_path,
    .rename = fat32_vfs_rename_path,
};

void fat32_vfs_reset(void) {
    for (uint32_t i = 0; i < FAT32_MAX_VFS_FILES; i++) {
        clear_vfs_slot(i);
    }
    g_fat32_vfs_count = 0;
    g_fat32_vfs_mount.fs = 0;

    g_fat32_vfs_registered =
        vfs_mount(FAT32_VFS_ROOT_PATH, &g_fat32_vfs_ops,
                  &g_fat32_vfs_mount) == 0;
}

int fat32_mount_vfs_root(fat32_fs_t *fs, const char *path) {
    if (g_fat32_vfs_registered == 0 || fs == 0 || fs->mounted == 0 ||
        path == 0 ||
        !((path[0] == '/') && (path[1] == 'f') && (path[2] == 'a') &&
          (path[3] == 't') && (path[4] == '\0'))) {
        return -1;
    }

    g_fat32_vfs_mount.fs = fs;
    return 0;
}

int fat32_mount_vfs_file(fat32_fs_t *fs, const char *path,
                         const char *name) {
    uint32_t index = g_fat32_vfs_count;

    if (fs == 0 || fs->mounted == 0 || path == 0 || path[0] != '/' ||
        name == 0 || index >= FAT32_MAX_VFS_FILES) {
        return -1;
    }

    clear_vfs_slot(index);
    if (fat32_open_path(fs, name, &g_fat32_vfs_files[index].file) != 0) {
        return -1;
    }

    g_fat32_vfs_files[index].fs = fs;
    g_fat32_vfs_nodes[index].path = path;
    g_fat32_vfs_nodes[index].size = g_fat32_vfs_files[index].file.size;
    g_fat32_vfs_nodes[index].read = fat32_vfs_read;
    g_fat32_vfs_nodes[index].write = fat32_vfs_write;
    g_fat32_vfs_nodes[index].stat = fat32_vfs_stat;
    g_fat32_vfs_nodes[index].context = &g_fat32_vfs_files[index];

    if (vfs_mount_static(&g_fat32_vfs_nodes[index], 1) != 0) {
        clear_vfs_slot(index);
        return -1;
    }

    g_fat32_vfs_count++;
    return 0;
}
