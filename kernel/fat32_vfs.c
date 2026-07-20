#include "kernel/fat32.h"

#include <stdint.h>

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

#ifdef ARMONIOS_TEST
    if (fs == 0) {
        /*
         * Compatibility for older host tests. Production builds require the
         * explicit fat32_mount_vfs_root() binding and never use this path.
         */
        fs = fat32_default_fs();
    }
#endif

    return fs != 0 && fs->mounted != 0 ? fs : 0;
}

static int fat32_vfs_root_name(const char *path, char *out,
                               uint64_t out_size) {
    const char *suffix = vfs_strip_prefix(path, FAT32_VFS_ROOT_PATH "/");
    uint64_t i;

    if (suffix == 0 || out == 0 || out_size == 0) {
        return -1;
    }

    for (i = 0; i + 1U < out_size; i++) {
        char c = suffix[i];

        if (c == '\0') {
            break;
        }
        if (c == '/') {
            return -1;
        }
        out[i] = c;
    }

    if (suffix[i] != '\0') {
        return -1;
    }
    out[i] = '\0';
    return i == 0 ? -1 : 0;
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
    fat32_fs_t *fs = fat32_vfs_fs(context);

    if (fs == 0) {
        return -1;
    }
    return fat32_list_root(fs, buffer, capacity, bytes_written);
}

static int fat32_vfs_open_path(void *context, const char *path,
                               uint32_t flags) {
    char name[VFS_MAX_PATH];
    fat32_file_t file;
    fat32_fs_t *fs = fat32_vfs_fs(context);

    if (fs == 0 ||
        fat32_vfs_root_name(path, name, sizeof(name)) != 0) {
        return -1;
    }

    if (fat32_open_root(fs, name, &file) != 0) {
        if ((flags & VFS_O_CREAT) == 0 ||
            fat32_create(fs, name, &file) != 0) {
            return -1;
        }
    }

    return fat32_mount_vfs_file(fs, path, name);
}

static int fat32_vfs_unlink_path(void *context, const char *path) {
    char name[VFS_MAX_PATH];
    fat32_fs_t *fs = fat32_vfs_fs(context);

    if (fs == 0 ||
        fat32_vfs_root_name(path, name, sizeof(name)) != 0) {
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
    .unlink = fat32_vfs_unlink_path,
    .rename = fat32_vfs_rename_path,
};

void fat32_vfs_reset(void) {
    for (uint32_t i = 0; i < FAT32_MAX_VFS_FILES; i++) {
        clear_vfs_slot(i);
    }
    g_fat32_vfs_count = 0;
    g_fat32_vfs_mount.fs = 0;

    /* FAT32 owns the current /fat policy; the generic VFS does not. */
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
