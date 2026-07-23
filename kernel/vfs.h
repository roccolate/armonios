#ifndef ARMONIOS_KERNEL_VFS_H
#define ARMONIOS_KERNEL_VFS_H

#include <stdint.h>

#include "include/armonios/abi/vfs.h"

#define VFS_MAX_NODES 24U
#define VFS_MAX_MOUNTS 4U
#define VFS_MAX_OPEN_FILES 8U
#define VFS_MAX_GLOBAL_OPEN_FILES (VFS_MAX_OPEN_FILES * 16U)
#define VFS_MAX_PATH 64U
#define VFS_NAME_MAX 64U
#define VFS_READDIR_MAX_ENTRIES 8U

#define VFS_O_RDONLY  ARM_O_RDONLY
#define VFS_O_WRONLY  ARM_O_WRONLY
#define VFS_O_RDWR    ARM_O_RDWR
#define VFS_O_ACCMODE ARM_O_ACCMODE
#define VFS_O_CREAT   ARM_O_CREAT
#define VFS_O_ALLOWED ARM_O_ALLOWED

#define VFS_FILE_TYPE_UNKNOWN   0U
#define VFS_FILE_TYPE_REGULAR   1U
#define VFS_FILE_TYPE_DIRECTORY 2U

#define VFS_ATTRIBUTE_READ_ONLY 0x01U
#define VFS_ATTRIBUTE_HIDDEN    0x02U
#define VFS_ATTRIBUTE_SYSTEM    0x04U
#define VFS_ATTRIBUTE_ARCHIVE   0x08U

typedef arm_stat_t vfs_stat_t;

typedef struct {
    uint64_t size;
    uint32_t type;
    uint32_t attributes;
} vfs_metadata_t;

typedef struct {
    char name[VFS_NAME_MAX];
    vfs_metadata_t metadata;
} vfs_dirent_t;

typedef int (*vfs_read_fn_t)(void *context, uint64_t offset, uint8_t *buffer,
                             uint64_t capacity, uint64_t *bytes_read);
typedef int (*vfs_write_fn_t)(void *context, uint64_t offset,
                              const uint8_t *buffer, uint64_t size,
                              uint64_t *bytes_written);
typedef int (*vfs_stat_fn_t)(void *context, vfs_stat_t *stat);
typedef int (*vfs_list_fn_t)(void *context, uint64_t offset, uint8_t *buffer,
                             uint64_t capacity, uint64_t *bytes_written);

typedef struct {
    const char *path;
    uint64_t size;
    vfs_read_fn_t read;
    vfs_write_fn_t write;
    vfs_stat_fn_t stat;
    void *context;
} vfs_node_t;

typedef int (*vfs_mount_open_fn_t)(void *context, const char *path,
                                   uint32_t flags);
typedef int (*vfs_mount_stat_path_fn_t)(void *context, const char *path,
                                        vfs_stat_t *stat);
typedef int (*vfs_mount_list_path_fn_t)(void *context, const char *path,
                                        uint64_t offset, uint8_t *buffer,
                                        uint64_t capacity,
                                        uint64_t *bytes_written);
typedef int (*vfs_mount_metadata_path_fn_t)(void *context, const char *path,
                                            vfs_metadata_t *metadata);
typedef int (*vfs_mount_readdir_path_fn_t)(void *context, const char *path,
                                           uint64_t start_index,
                                           vfs_dirent_t *entries,
                                           uint64_t max_entries,
                                           uint64_t *entries_written);
typedef int (*vfs_mount_unlink_fn_t)(void *context, const char *path);
typedef int (*vfs_mount_rename_fn_t)(void *context, const char *old_path,
                                     const char *new_path);

typedef struct {
    vfs_mount_open_fn_t open;
    vfs_list_fn_t list;
    vfs_mount_stat_path_fn_t stat_path;
    vfs_mount_list_path_fn_t list_path;
    vfs_mount_metadata_path_fn_t metadata_path;
    vfs_mount_readdir_path_fn_t readdir_path;
    vfs_mount_unlink_fn_t unlink;
    vfs_mount_rename_fn_t rename;
} vfs_mount_ops_t;

void vfs_reset(void);
int vfs_normalize_path(const char *path, char normalized[VFS_MAX_PATH]);
int vfs_mount_static(const vfs_node_t *nodes, uint32_t count);
int vfs_unmount_static(const char *path);
int vfs_mount(const char *path, const vfs_mount_ops_t *ops, void *context);
int vfs_mount_list(const char *path, vfs_list_fn_t list, void *context);
const vfs_node_t *vfs_find(const char *path);
const char *vfs_strip_prefix(const char *path, const char *prefix);
int vfs_read(const char *path, uint64_t offset, uint8_t *buffer,
             uint64_t capacity, uint64_t *bytes_read);
int vfs_write(const char *path, uint64_t offset, const uint8_t *buffer,
              uint64_t size, uint64_t *bytes_written);
int vfs_stat(const char *path, vfs_stat_t *stat);
int vfs_list_at(const char *path, uint64_t offset, uint8_t *buffer,
                uint64_t capacity, uint64_t *bytes_written);
int vfs_list(const char *path, uint8_t *buffer, uint64_t capacity,
             uint64_t *bytes_written);

/* Filesystem-neutral structured metadata. */
int vfs_metadata(const char *path, vfs_metadata_t *metadata);
int vfs_readdir(const char *path, uint64_t start_index,
                vfs_dirent_t *entries, uint64_t max_entries,
                uint64_t *entries_written);

/* Public ABI adapters. The global ABI remains 1.0 during pre-release work. */
int vfs_stat_v2(const char *path, arm_stat_v2_t *stat);
int vfs_readdir_v2(const char *path, uint64_t start_index,
                   arm_dirent_v2_t *entries, uint64_t max_entries,
                   uint64_t *entries_written);

int vfs_open(const char *path);
int vfs_open_flags(const char *path, uint32_t flags);
int vfs_read_fd(int fd, uint8_t *buffer, uint64_t capacity,
                uint64_t *bytes_read);
int vfs_write_fd(int fd, const uint8_t *buffer, uint64_t size,
                 uint64_t *bytes_written);
int vfs_seek(int fd, uint64_t offset);
int vfs_close(int fd);
uint32_t vfs_close_all_for_pid(uint32_t pid);
int vfs_unlink(const char *path);
int vfs_rename(const char *old_path, const char *new_path);

#endif
