#ifndef ARMONIOS_KERNEL_FAT32_H
#define ARMONIOS_KERNEL_FAT32_H

#include <stdint.h>

#include "storage/block_device.h"

#define FAT32_SECTOR_SIZE 512U
#define FAT32_SHORT_NAME_MAX 13U

#define FAT32_ATTR_READ_ONLY 0x01U
#define FAT32_ATTR_HIDDEN    0x02U
#define FAT32_ATTR_SYSTEM    0x04U
#define FAT32_ATTR_VOLUME_ID 0x08U
#define FAT32_ATTR_DIRECTORY 0x10U
#define FAT32_ATTR_ARCHIVE   0x20U

/*
 * FAT32 storage facade.
 *
 * The canonical mount path receives a finite block_device_t. The legacy
 * single-sector callback path remains temporarily for host tests and staged
 * migration, but production storage should use fat32_mount_device().
 */

typedef int (*fat32_read_sector_fn_t)(void *context, uint32_t lba,
                                       uint8_t *buffer);
typedef int (*fat32_write_sector_fn_t)(void *context, uint32_t lba,
                                        const uint8_t *buffer);

typedef struct {
    fat32_read_sector_fn_t read_sector;
    fat32_write_sector_fn_t write_sector;
    void *context;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t fat_count;
    uint32_t sectors_per_fat;
    uint32_t total_sectors;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    uint32_t root_cluster;
    uint32_t cluster_count;
    uint8_t sector[FAT32_SECTOR_SIZE];
    uint8_t write_sector_buffer[FAT32_SECTOR_SIZE];
    uint8_t mounted;
} fat32_fs_t;

typedef struct {
    uint32_t first_cluster;
    uint32_t dir_lba;
    uint32_t dir_offset;
    uint32_t capacity;
    uint32_t size;
} fat32_file_t;

typedef struct {
    uint32_t first_cluster;
    uint32_t dir_lba;
    uint32_t dir_offset;
    uint32_t capacity;
    uint32_t size;
    uint8_t attributes;
} fat32_path_info_t;

typedef struct {
    char name[FAT32_SHORT_NAME_MAX];
    uint32_t size;
    uint8_t attributes;
} fat32_dirent_t;

int fat32_mount_device(fat32_fs_t *fs, const block_device_t *device);
int fat32_flush(fat32_fs_t *fs);

/* Compatibility path for callback-backed host fixtures. */
int fat32_mount(fat32_fs_t *fs, fat32_read_sector_fn_t read_sector,
                 void *context);
void fat32_set_write_sector(fat32_fs_t *fs,
                             fat32_write_sector_fn_t write_sector);

/* Existing root-only mutation compatibility API. */
int fat32_open_root(fat32_fs_t *fs, const char *name, fat32_file_t *file);
int fat32_list_root_at(fat32_fs_t *fs, uint64_t offset, uint8_t *buffer,
                        uint64_t capacity, uint64_t *bytes_written);
int fat32_list_root(fat32_fs_t *fs, uint8_t *buffer, uint64_t capacity,
                     uint64_t *bytes_written);
int fat32_create(fat32_fs_t *fs, const char *name, fat32_file_t *file);
int fat32_delete(fat32_fs_t *fs, const char *name);
int fat32_rename(fat32_fs_t *fs, const char *old_name,
                  const char *new_name);

/*
 * Read-only component traversal. Paths are relative to the FAT volume root and
 * use 8.3 components. A leading slash is accepted. Directories can be looked
 * up and listed; fat32_open_path() rejects a directory final component.
 */
int fat32_lookup_path(fat32_fs_t *fs, const char *path,
                      fat32_path_info_t *info);
int fat32_open_path(fat32_fs_t *fs, const char *path, fat32_file_t *file);
int fat32_readdir_path(fat32_fs_t *fs, const char *path,
                       uint64_t start_index, fat32_dirent_t *entries,
                       uint64_t max_entries, uint64_t *entries_written);
int fat32_list_path_at(fat32_fs_t *fs, const char *path, uint64_t offset,
                       uint8_t *buffer, uint64_t capacity,
                       uint64_t *bytes_written);
int fat32_list_path(fat32_fs_t *fs, const char *path, uint8_t *buffer,
                    uint64_t capacity, uint64_t *bytes_written);

int fat32_read(fat32_fs_t *fs, const fat32_file_t *file, uint64_t offset,
                uint8_t *buffer, uint64_t capacity, uint64_t *bytes_read);
int fat32_write(fat32_fs_t *fs, fat32_file_t *file, uint64_t offset,
                 const uint8_t *buffer, uint64_t size,
                 uint64_t *bytes_written);

void fat32_vfs_reset(void);
int fat32_mount_vfs_root(fat32_fs_t *fs, const char *path);
int fat32_mount_vfs_file(fat32_fs_t *fs, const char *path,
                          const char *name);

#endif
