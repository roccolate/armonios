#include "kernel/fat32.h"

#include <stdint.h>

#include "kernel/kstring.h"

#define FAT32_ATTR_VOLUME_ID 0x08U
#define FAT32_ATTR_LONG_NAME 0x0fU
#define FAT32_CLUSTER_MASK   0x0fffffffU
#define FAT32_CLUSTER_EOC    0x0ffffff8U
#define FAT32_CLUSTER_BAD    0x0ffffff7U
#define FAT32_COMPONENT_MAX  12U

static uint16_t dir_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t dir_le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int dir_cluster_is_eoc(uint32_t cluster) {
    return cluster >= FAT32_CLUSTER_EOC;
}

static int dir_cluster_is_data(const fat32_fs_t *fs, uint32_t cluster) {
    return fs != 0 && cluster >= 2U &&
           cluster - 2U < fs->cluster_count;
}

static uint32_t dir_cluster_to_lba(const fat32_fs_t *fs, uint32_t cluster) {
    return fs->data_start_lba +
           (cluster - 2U) * fs->sectors_per_cluster;
}

static int dir_read_sector(fat32_fs_t *fs, uint32_t lba) {
    if (fs == 0 || fs->mounted == 0 || fs->read_sector == 0 ||
        lba >= fs->total_sectors) {
        return -1;
    }
    return fs->read_sector(fs->context, lba, fs->sector);
}

static int dir_read_fat_entry(fat32_fs_t *fs, uint32_t cluster,
                              uint32_t *next) {
    uint32_t fat_offset;
    uint32_t sector;
    uint32_t offset;
    uint32_t value;

    if (next == 0 || !dir_cluster_is_data(fs, cluster)) {
        return -1;
    }

    fat_offset = cluster * 4U;
    sector = fs->fat_start_lba + fat_offset / FAT32_SECTOR_SIZE;
    offset = fat_offset % FAT32_SECTOR_SIZE;
    if (dir_read_sector(fs, sector) != 0) {
        return -1;
    }

    value = dir_le32(&fs->sector[offset]) & FAT32_CLUSTER_MASK;
    if (value == FAT32_CLUSTER_BAD ||
        (!dir_cluster_is_eoc(value) && !dir_cluster_is_data(fs, value))) {
        return -1;
    }
    *next = value;
    return 0;
}

static int dir_ascii_upper(int c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

static int dir_make_short_name(const char *name, uint8_t out[11]) {
    uint32_t i = 0;
    uint32_t base = 0;
    uint32_t ext = 0;
    uint8_t in_ext = 0;

    if (name == 0 || name[0] == '\0') {
        return -1;
    }

    (void)kmemset(out, ' ', 11U);
    while (name[i] != '\0') {
        int c = dir_ascii_upper(name[i]);

        if (c == '/' || c == '\\') {
            return -1;
        }
        if (c == '.') {
            if (in_ext != 0 || base == 0) {
                return -1;
            }
            in_ext = 1;
            i++;
            continue;
        }
        if (c <= ' ' || c == '"' || c == '*' || c == '+' || c == ',' ||
            c == ':' || c == ';' || c == '<' || c == '=' || c == '>' ||
            c == '?' || c == '[' || c == ']' || c == '|') {
            return -1;
        }

        if (in_ext == 0) {
            if (base >= 8U) {
                return -1;
            }
            out[base++] = (uint8_t)c;
        } else {
            if (ext >= 3U) {
                return -1;
            }
            out[8U + ext++] = (uint8_t)c;
        }
        i++;
    }

    return base != 0U && (in_ext == 0U || ext != 0U) ? 0 : -1;
}

static int dir_file_capacity(fat32_fs_t *fs, uint32_t first_cluster,
                             uint32_t *capacity) {
    uint64_t total = 0;
    uint32_t cluster = first_cluster;
    uint32_t remaining;
    uint32_t cluster_size;

    if (capacity == 0 || !dir_cluster_is_data(fs, first_cluster)) {
        return -1;
    }

    remaining = fs->cluster_count;
    cluster_size = fs->sectors_per_cluster * FAT32_SECTOR_SIZE;
    while (remaining-- != 0U) {
        total += cluster_size;
        if (total > UINT32_MAX ||
            dir_read_fat_entry(fs, cluster, &cluster) != 0) {
            return -1;
        }
        if (dir_cluster_is_eoc(cluster)) {
            *capacity = (uint32_t)total;
            return 0;
        }
    }
    return -1;
}

static int dir_fill_info(fat32_fs_t *fs, const uint8_t *entry,
                         uint32_t lba, uint32_t offset,
                         fat32_path_info_t *info) {
    uint32_t first_cluster;
    uint32_t size;
    uint8_t attributes;

    if (entry == 0 || info == 0) {
        return -1;
    }

    first_cluster = ((uint32_t)dir_le16(&entry[20]) << 16) |
                    dir_le16(&entry[26]);
    size = dir_le32(&entry[28]);
    attributes = entry[11];

    info->first_cluster = first_cluster;
    info->dir_lba = lba;
    info->dir_offset = offset;
    info->capacity = 0;
    info->size = size;
    info->attributes = attributes;

    if ((attributes & FAT32_ATTR_DIRECTORY) != 0U) {
        return dir_cluster_is_data(fs, first_cluster) ? 0 : -1;
    }
    if (size != 0U && !dir_cluster_is_data(fs, first_cluster)) {
        return -1;
    }
    if (dir_cluster_is_data(fs, first_cluster) &&
        (dir_file_capacity(fs, first_cluster, &info->capacity) != 0 ||
         size > info->capacity)) {
        return -1;
    }
    return 0;
}

static int dir_find_entry(fat32_fs_t *fs, uint32_t directory_cluster,
                          const char *component, fat32_path_info_t *info) {
    uint8_t wanted[11];
    uint32_t cluster = directory_cluster;
    uint32_t remaining;

    if (!dir_cluster_is_data(fs, directory_cluster) ||
        dir_make_short_name(component, wanted) != 0) {
        return -1;
    }

    remaining = fs->cluster_count;
    while (dir_cluster_is_data(fs, cluster) && remaining-- != 0U) {
        for (uint32_t sector = 0; sector < fs->sectors_per_cluster; sector++) {
            uint32_t lba = dir_cluster_to_lba(fs, cluster) + sector;

            if (dir_read_sector(fs, lba) != 0) {
                return -1;
            }
            for (uint32_t offset = 0; offset < FAT32_SECTOR_SIZE;
                 offset += 32U) {
                const uint8_t *entry = &fs->sector[offset];
                uint8_t attr = entry[11];

                if (entry[0] == 0x00U) {
                    return -1;
                }
                if (entry[0] == 0xe5U || attr == FAT32_ATTR_LONG_NAME ||
                    (attr & FAT32_ATTR_VOLUME_ID) != 0U) {
                    continue;
                }
                if (kmemcmp(entry, wanted, 11U) == 0) {
                    return dir_fill_info(fs, entry, lba, offset, info);
                }
            }
        }
        if (dir_read_fat_entry(fs, cluster, &cluster) != 0) {
            return -1;
        }
    }
    return -1;
}

static int dir_next_component(const char **cursor,
                              char component[FAT32_COMPONENT_MAX + 1U],
                              uint8_t *has_more) {
    const char *p;
    uint32_t length = 0;

    if (cursor == 0 || *cursor == 0 || component == 0 || has_more == 0) {
        return -1;
    }

    p = *cursor;
    while (*p == '/') {
        p++;
    }
    if (*p == '\0') {
        return -1;
    }
    while (p[length] != '\0' && p[length] != '/') {
        if (length >= FAT32_COMPONENT_MAX) {
            return -1;
        }
        component[length] = p[length];
        length++;
    }
    component[length] = '\0';
    p += length;
    while (*p == '/') {
        p++;
    }
    *has_more = *p != '\0';
    *cursor = p;
    return length != 0U ? 0 : -1;
}

int fat32_lookup_path(fat32_fs_t *fs, const char *path,
                      fat32_path_info_t *info) {
    const char *cursor;
    uint32_t directory_cluster;

    if (fs == 0 || fs->mounted == 0 || path == 0 || info == 0) {
        return -1;
    }

    cursor = path;
    while (*cursor == '/') {
        cursor++;
    }
    if (*cursor == '\0') {
        info->first_cluster = fs->root_cluster;
        info->dir_lba = 0;
        info->dir_offset = 0;
        info->capacity = 0;
        info->size = 0;
        info->attributes = FAT32_ATTR_DIRECTORY;
        return 0;
    }

    directory_cluster = fs->root_cluster;
    for (;;) {
        char component[FAT32_COMPONENT_MAX + 1U];
        fat32_path_info_t current;
        uint8_t has_more;

        if (dir_next_component(&cursor, component, &has_more) != 0 ||
            dir_find_entry(fs, directory_cluster, component, &current) != 0) {
            return -1;
        }
        if (has_more == 0U) {
            *info = current;
            return 0;
        }
        if ((current.attributes & FAT32_ATTR_DIRECTORY) == 0U ||
            !dir_cluster_is_data(fs, current.first_cluster)) {
            return -1;
        }
        directory_cluster = current.first_cluster;
    }
}

int fat32_open_path(fat32_fs_t *fs, const char *path, fat32_file_t *file) {
    fat32_path_info_t info;

    if (file == 0 || fat32_lookup_path(fs, path, &info) != 0 ||
        (info.attributes & FAT32_ATTR_DIRECTORY) != 0U) {
        return -1;
    }

    file->first_cluster = info.first_cluster;
    file->dir_lba = info.dir_lba;
    file->dir_offset = info.dir_offset;
    file->capacity = info.capacity;
    file->size = info.size;
    return 0;
}

typedef struct {
    uint8_t *buffer;
    uint64_t capacity;
    uint64_t offset;
    uint64_t position;
    uint64_t written;
} dir_list_output_t;

static int dir_append_byte(dir_list_output_t *out, uint8_t value) {
    if (out->position >= out->offset) {
        if (out->written >= out->capacity) {
            return -1;
        }
        out->buffer[out->written++] = value;
    }
    out->position++;
    return 0;
}

static int dir_is_dot_entry(const uint8_t entry[11]) {
    return entry[0] == '.' &&
           (entry[1] == ' ' || (entry[1] == '.' && entry[2] == ' '));
}

static int dir_append_name(const uint8_t entry[11], uint8_t attr,
                           dir_list_output_t *out) {
    uint32_t base_end = 8U;
    uint32_t ext_end = 3U;

    while (base_end > 0U && entry[base_end - 1U] == ' ') {
        base_end--;
    }
    while (ext_end > 0U && entry[8U + ext_end - 1U] == ' ') {
        ext_end--;
    }
    for (uint32_t i = 0; i < base_end; i++) {
        if (dir_append_byte(out, entry[i]) != 0) {
            return -1;
        }
    }
    if (ext_end > 0U) {
        if (dir_append_byte(out, '.') != 0) {
            return -1;
        }
        for (uint32_t i = 0; i < ext_end; i++) {
            if (dir_append_byte(out, entry[8U + i]) != 0) {
                return -1;
            }
        }
    }
    if ((attr & FAT32_ATTR_DIRECTORY) != 0U &&
        dir_append_byte(out, '/') != 0) {
        return -1;
    }
    return dir_append_byte(out, '\n');
}

int fat32_list_path_at(fat32_fs_t *fs, const char *path, uint64_t offset,
                       uint8_t *buffer, uint64_t capacity,
                       uint64_t *bytes_written) {
    fat32_path_info_t info;
    dir_list_output_t out = {
        .buffer = buffer,
        .capacity = capacity,
        .offset = offset,
        .position = 0,
        .written = 0,
    };
    uint32_t cluster;
    uint32_t remaining;

    if (bytes_written != 0) {
        *bytes_written = 0;
    }
    if (buffer == 0 || bytes_written == 0 ||
        fat32_lookup_path(fs, path, &info) != 0 ||
        (info.attributes & FAT32_ATTR_DIRECTORY) == 0U) {
        return -1;
    }

    cluster = info.first_cluster;
    remaining = fs->cluster_count;
    while (dir_cluster_is_data(fs, cluster) && remaining-- != 0U) {
        for (uint32_t sector = 0; sector < fs->sectors_per_cluster; sector++) {
            uint32_t lba = dir_cluster_to_lba(fs, cluster) + sector;

            if (dir_read_sector(fs, lba) != 0) {
                return -1;
            }
            for (uint32_t entry_offset = 0;
                 entry_offset < FAT32_SECTOR_SIZE; entry_offset += 32U) {
                const uint8_t *entry = &fs->sector[entry_offset];
                uint8_t attr = entry[11];

                if (entry[0] == 0x00U) {
                    *bytes_written = out.written;
                    return 0;
                }
                if (entry[0] == 0xe5U || attr == FAT32_ATTR_LONG_NAME ||
                    (attr & FAT32_ATTR_VOLUME_ID) != 0U ||
                    dir_is_dot_entry(entry)) {
                    continue;
                }
                if (dir_append_name(entry, attr, &out) != 0) {
                    *bytes_written = out.written;
                    return 0;
                }
            }
        }
        if (dir_read_fat_entry(fs, cluster, &cluster) != 0) {
            return -1;
        }
    }

    if (!dir_cluster_is_eoc(cluster)) {
        return -1;
    }
    *bytes_written = out.written;
    return 0;
}

int fat32_list_path(fat32_fs_t *fs, const char *path, uint8_t *buffer,
                    uint64_t capacity, uint64_t *bytes_written) {
    return fat32_list_path_at(fs, path, 0, buffer, capacity, bytes_written);
}
