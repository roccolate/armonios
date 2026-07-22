#include "kernel/fat32.h"

#include <stdint.h>

#include "kernel/vfs.h"

#define FAT32_MAX_VFS_FILES 8U

typedef struct {
    fat32_fs_t *fs;
    fat32_file_t file;
} fat32_vfs_file_t;

static fat32_vfs_file_t g_fat32_vfs_files[FAT32_MAX_VFS_FILES];
static vfs_node_t g_fat32_vfs_nodes[FAT32_MAX_VFS_FILES];
static uint32_t g_fat32_vfs_count;

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

static int fat32_vfs_list_root(void *context, uint8_t *buffer,
                               uint64_t capacity, uint64_t *bytes_written) {
    return fat32_list_root((fat32_fs_t *)context, buffer, capacity,
                           bytes_written);
}

void fat32_vfs_reset(void) {
    for (uint32_t i = 0; i < FAT32_MAX_VFS_FILES; i++) {
        clear_vfs_slot(i);
    }
    g_fat32_vfs_count = 0;
}

int fat32_mount_vfs_root(fat32_fs_t *fs, const char *path) {
    if (fs == 0 || fs->mounted == 0 || path == 0 || path[0] != '/') {
        return -1;
    }

    return vfs_mount_list(path, fat32_vfs_list_root, fs);
}

int fat32_mount_vfs_file(fat32_fs_t *fs, const char *path,
                         const char *name) {
    uint32_t index = g_fat32_vfs_count;

    if (fs == 0 || fs->mounted == 0 || path == 0 || path[0] != '/' ||
        name == 0 || index >= FAT32_MAX_VFS_FILES) {
        return -1;
    }

    clear_vfs_slot(index);
    if (fat32_open_root(fs, name, &g_fat32_vfs_files[index].file) != 0) {
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
