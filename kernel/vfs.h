#ifndef ARMONIOS_KERNEL_VFS_H
#define ARMONIOS_KERNEL_VFS_H

#include <stdint.h>

#include "include/armonios/abi/vfs.h"

#define VFS_MAX_NODES 24U
#define VFS_MAX_MOUNTS 4U
/*
 * Descriptors returned by vfs_open* are local to the current process.
 * The internal handle pool is larger and never exposed to EL0.
 */
#define VFS_MAX_OPEN_FILES 8U
#define VFS_MAX_GLOBAL_OPEN_FILES (VFS_MAX_OPEN_FILES * 16U)
#define VFS_MAX_PATH 64U

/* Historical kernel spellings retained as aliases of the public ABI. */
#define VFS_O_RDONLY  ARM_O_RDONLY
#define VFS_O_WRONLY  ARM_O_WRONLY
#define VFS_O_RDWR    ARM_O_RDWR
#define VFS_O_ACCMODE ARM_O_ACCMODE
#define VFS_O_CREAT   ARM_O_CREAT
#define VFS_O_ALLOWED ARM_O_ALLOWED

/*
 * Fixed-table kernel VFS facade.
 *
 * Paths are absolute, canonicalized into VFS-owned storage at mount time, and
 * capped by VFS_MAX_PATH. File descriptors are local to the current process;
 * the VFS translates them to kernel-private open-file handles and owns their
 * offsets.
 */
typedef arm_stat_t vfs_stat_t;

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

/*
 * A mount owns operations below one absolute prefix. Path-aware callbacks
 * receive the canonical absolute path selected by the VFS. The legacy `list`
 * callback remains for mounts that only expose their exact root.
 */
typedef int (*vfs_mount_open_fn_t)(void *context, const char *path,
                                   uint32_t flags);
typedef int (*vfs_mount_stat_path_fn_t)(void *context, const char *path,
                                        vfs_stat_t *stat);
typedef int (*vfs_mount_list_path_fn_t)(void *context, const char *path,
                                        uint64_t offset, uint8_t *buffer,
                                        uint64_t capacity,
                                        uint64_t *bytes_written);
typedef int (*vfs_mount_unlink_fn_t)(void *context, const char *path);
typedef int (*vfs_mount_rename_fn_t)(void *context, const char *old_path,
                                     const char *new_path);

typedef struct {
    vfs_mount_open_fn_t open;
    vfs_list_fn_t list;
    vfs_mount_stat_path_fn_t stat_path;
    vfs_mount_list_path_fn_t list_path;
    vfs_mount_unlink_fn_t unlink;
    vfs_mount_rename_fn_t rename;
} vfs_mount_ops_t;

void vfs_reset(void);

/*
 * Convert one bounded absolute path into the unique VFS representation.
 * Repeated separators and '.' are removed, '..' pops one component, attempts
 * to escape above '/' are rejected, and only '/' retains a trailing slash.
 */
int vfs_normalize_path(const char *path, char normalized[VFS_MAX_PATH]);

int vfs_mount_static(const vfs_node_t *nodes, uint32_t count);

/*
 * Remove a mounted static VFS node by path. Open handles pointing at that node
 * are invalidated so later descriptor operations fail cleanly instead of
 * touching stale filesystem state. Mount roots are not affected.
 */
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
int vfs_open(const char *path);
int vfs_open_flags(const char *path, uint32_t flags);
int vfs_read_fd(int fd, uint8_t *buffer, uint64_t capacity,
                uint64_t *bytes_read);
int vfs_write_fd(int fd, const uint8_t *buffer, uint64_t size,
                 uint64_t *bytes_written);
int vfs_seek(int fd, uint64_t offset);
int vfs_close(int fd);
uint32_t vfs_close_all_for_pid(uint32_t pid);

/*
 * Mutation is dispatched through the mount that owns the canonical path.
 * Filesystems without unlink or rename callbacks reject the operation.
 */
int vfs_unlink(const char *path);
int vfs_rename(const char *old_path, const char *new_path);

#endif
