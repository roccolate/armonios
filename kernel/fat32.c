#include "kernel/fat32.h"

#include <stdint.h>

/*
 * Minimal FAT32 root-directory support.
 *
 * Every on-disk cluster is checked against geometry established by mount.
 * Chain walks use cluster_count as a hard budget, so cyclic or malformed FAT
 * data cannot keep EL1 in an unbounded loop.
 */

#define FAT32_ATTR_VOLUME_ID 0x08U
#define FAT32_ATTR_DIRECTORY 0x10U
#define FAT32_ATTR_LONG_NAME 0x0fU
#define FAT32_CLUSTER_MASK   0x0fffffffU
#define FAT32_CLUSTER_BAD    0x0ffffff7U
#define FAT32_CLUSTER_EOC    0x0ffffff8U
#define FAT32_CLUSTER_RSVD   0x0ffffff0U

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put_le16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static int cluster_is_eoc(uint32_t cluster) {
    return cluster >= FAT32_CLUSTER_EOC;
}

static int cluster_is_data(const fat32_fs_t *fs, uint32_t cluster) {
    return fs != 0 && fs->mounted != 0 && cluster >= 2U &&
           cluster < fs->max_cluster;
}

static int read_sector(fat32_fs_t *fs, uint32_t lba) {
    if (fs == 0 || fs->read_sector == 0 ||
        (fs->mounted != 0 && lba >= fs->total_sectors)) {
        return -1;
    }
    return fs->read_sector(fs->context, lba, fs->sector);
}

static int write_sector(fat32_fs_t *fs, uint32_t lba,
                        const uint8_t *buffer) {
    if (fs == 0 || fs->mounted == 0 || fs->write_sector == 0 ||
        buffer == 0 || lba >= fs->total_sectors) {
        return -1;
    }
    return fs->write_sector(fs->context, lba, buffer);
}

static int cluster_lba(const fat32_fs_t *fs, uint32_t cluster,
                       uint32_t *lba) {
    uint64_t first;
    uint64_t last;

    if (lba == 0 || !cluster_is_data(fs, cluster)) {
        return -1;
    }

    first = (uint64_t)fs->data_start_lba +
            (uint64_t)(cluster - 2U) * fs->sectors_per_cluster;
    last = first + fs->sectors_per_cluster;
    if (first > UINT32_MAX || last > fs->total_sectors) {
        return -1;
    }

    *lba = (uint32_t)first;
    return 0;
}

static int fat_location(const fat32_fs_t *fs, uint32_t cluster,
                        uint32_t copy, uint32_t *lba,
                        uint32_t *entry_offset) {
    uint64_t offset;
    uint64_t sector;
    uint64_t base;

    if (lba == 0 || entry_offset == 0 || !cluster_is_data(fs, cluster) ||
        copy >= fs->fat_count) {
        return -1;
    }

    offset = (uint64_t)cluster * 4U;
    sector = offset / FAT32_SECTOR_SIZE;
    if (sector >= fs->sectors_per_fat) {
        return -1;
    }

    base = (uint64_t)fs->fat_start_lba +
           (uint64_t)copy * fs->sectors_per_fat;
    if (base + sector >= fs->data_start_lba ||
        base + sector >= fs->total_sectors) {
        return -1;
    }

    *lba = (uint32_t)(base + sector);
    *entry_offset = (uint32_t)(offset % FAT32_SECTOR_SIZE);
    return 0;
}

static int read_fat_raw(fat32_fs_t *fs, uint32_t cluster, uint32_t *value) {
    uint32_t lba;
    uint32_t offset;

    if (value == 0 || fat_location(fs, cluster, 0, &lba, &offset) != 0 ||
        read_sector(fs, lba) != 0) {
        return -1;
    }

    *value = le32(&fs->sector[offset]) & FAT32_CLUSTER_MASK;
    return 0;
}

static int read_chain_next(fat32_fs_t *fs, uint32_t cluster,
                           uint32_t *next) {
    uint32_t value;

    if (next == 0 || read_fat_raw(fs, cluster, &value) != 0 ||
        value == FAT32_CLUSTER_BAD ||
        (value >= FAT32_CLUSTER_RSVD && !cluster_is_eoc(value))) {
        return -1;
    }

    if (!cluster_is_eoc(value) && !cluster_is_data(fs, value)) {
        return -1;
    }

    *next = value;
    return 0;
}

static int write_fat_entry(fat32_fs_t *fs, uint32_t cluster,
                           uint32_t value) {
    if (!cluster_is_data(fs, cluster) ||
        (!cluster_is_eoc(value) && value != 0 &&
         !cluster_is_data(fs, value))) {
        return -1;
    }

    for (uint32_t copy = 0; copy < fs->fat_count; copy++) {
        uint32_t lba;
        uint32_t offset;

        if (fat_location(fs, cluster, copy, &lba, &offset) != 0 ||
            read_sector(fs, lba) != 0) {
            return -1;
        }
        put_le32(&fs->sector[offset], value & FAT32_CLUSTER_MASK);
        if (write_sector(fs, lba, fs->sector) != 0) {
            return -1;
        }
    }
    return 0;
}

static int ascii_upper(int c) {
    return c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
}

static int make_short_name(const char *name, uint8_t out[11]) {
    uint32_t i = 0;
    uint32_t base = 0;
    uint32_t ext = 0;
    uint8_t in_ext = 0;

    if (name == 0 || name[0] == '\0') {
        return -1;
    }
    if (name[0] == '/') {
        name++;
        if (name[0] == '\0') {
            return -1;
        }
    }

    for (uint32_t j = 0; j < 11U; j++) {
        out[j] = ' ';
    }

    while (name[i] != '\0') {
        int c = ascii_upper(name[i++]);

        if (c == '/' || c <= ' ' || c == '"' || c == '*' || c == '+' ||
            c == ',' || c == ':' || c == ';' || c == '<' || c == '=' ||
            c == '>' || c == '?' || c == '[' || c == '\\' || c == ']' ||
            c == '|') {
            return -1;
        }
        if (c == '.') {
            if (in_ext != 0 || base == 0) {
                return -1;
            }
            in_ext = 1;
            continue;
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
    }

    return base != 0 && (in_ext == 0 || ext != 0) ? 0 : -1;
}

static int short_name_equals(const uint8_t entry[11],
                             const uint8_t wanted[11]) {
    for (uint32_t i = 0; i < 11U; i++) {
        if (entry[i] != wanted[i]) {
            return 0;
        }
    }
    return 1;
}

typedef struct {
    uint8_t *buffer;
    uint64_t capacity;
    uint64_t offset;
    uint64_t position;
    uint64_t written;
} fat32_list_output_t;

static int append_list_byte(fat32_list_output_t *out, uint8_t value) {
    if (out->position >= out->offset) {
        if (out->written >= out->capacity) {
            return -1;
        }
        out->buffer[out->written++] = value;
    }
    out->position++;
    return 0;
}

static int append_short_name(const uint8_t entry[11], uint8_t attr,
                             fat32_list_output_t *out) {
    uint32_t base_end = 8U;
    uint32_t ext_end = 3U;

    while (base_end != 0 && entry[base_end - 1U] == ' ') {
        base_end--;
    }
    while (ext_end != 0 && entry[8U + ext_end - 1U] == ' ') {
        ext_end--;
    }
    for (uint32_t i = 0; i < base_end; i++) {
        if (append_list_byte(out, entry[i]) != 0) {
            return -1;
        }
    }
    if (ext_end != 0) {
        if (append_list_byte(out, '.') != 0) {
            return -1;
        }
        for (uint32_t i = 0; i < ext_end; i++) {
            if (append_list_byte(out, entry[8U + i]) != 0) {
                return -1;
            }
        }
    }
    if ((attr & FAT32_ATTR_DIRECTORY) != 0 &&
        append_list_byte(out, '/') != 0) {
        return -1;
    }
    return append_list_byte(out, '\n');
}

static int file_chain_capacity(fat32_fs_t *fs, uint32_t first_cluster,
                               uint32_t *capacity,
                               uint32_t *chain_length) {
    uint64_t total = 0;
    uint64_t cluster_size;
    uint32_t cluster;

    if (capacity == 0 || !cluster_is_data(fs, first_cluster)) {
        return -1;
    }

    cluster = first_cluster;
    cluster_size = (uint64_t)fs->sectors_per_cluster * FAT32_SECTOR_SIZE;
    for (uint32_t steps = 0; steps < fs->cluster_count; steps++) {
        uint32_t next;

        if (!cluster_is_data(fs, cluster)) {
            return -1;
        }
        total += cluster_size;
        if (total > UINT32_MAX ||
            read_chain_next(fs, cluster, &next) != 0) {
            return -1;
        }
        if (cluster_is_eoc(next)) {
            *capacity = (uint32_t)total;
            if (chain_length != 0) {
                *chain_length = steps + 1U;
            }
            return 0;
        }
        cluster = next;
    }
    return -1;
}

static uint32_t find_free_cluster(fat32_fs_t *fs) {
    for (uint32_t cluster = 2U; cluster < fs->max_cluster; cluster++) {
        uint32_t value;

        if (read_fat_raw(fs, cluster, &value) != 0) {
            return 0;
        }
        if (value == 0) {
            return cluster;
        }
    }
    return 0;
}

static uint32_t allocate_cluster(fat32_fs_t *fs, uint32_t previous) {
    uint32_t cluster = find_free_cluster(fs);
    uint32_t value;

    if (cluster == 0 ||
        write_fat_entry(fs, cluster, FAT32_CLUSTER_EOC) != 0) {
        return 0;
    }

    if (previous != 0) {
        if (!cluster_is_data(fs, previous) ||
            read_fat_raw(fs, previous, &value) != 0 ||
            !cluster_is_eoc(value) ||
            write_fat_entry(fs, previous, cluster) != 0) {
            (void)write_fat_entry(fs, cluster, 0);
            return 0;
        }
    }
    return cluster;
}

static int advance_or_allocate(fat32_fs_t *fs, fat32_file_t *file,
                               uint64_t cluster_size, uint32_t *cluster) {
    uint32_t next;

    if (cluster == 0 || !cluster_is_data(fs, *cluster) ||
        read_chain_next(fs, *cluster, &next) != 0) {
        return -1;
    }

    if (!cluster_is_eoc(next)) {
        *cluster = next;
        return 0;
    }

    if (cluster_size > UINT32_MAX ||
        file->capacity > UINT32_MAX - (uint32_t)cluster_size) {
        return -1;
    }

    next = allocate_cluster(fs, *cluster);
    if (next == 0) {
        return -1;
    }
    file->capacity += (uint32_t)cluster_size;
    *cluster = next;
    return 0;
}

static int free_cluster_chain(fat32_fs_t *fs, uint32_t first_cluster) {
    uint32_t count;
    uint32_t ignored;
    uint32_t cluster = first_cluster;

    /* Preflight the entire chain before mutating any FAT entry. */
    if (file_chain_capacity(fs, first_cluster, &ignored, &count) != 0) {
        return -1;
    }

    for (uint32_t i = 0; i < count; i++) {
        uint32_t next;

        if (read_chain_next(fs, cluster, &next) != 0 ||
            write_fat_entry(fs, cluster, 0) != 0) {
            return -1;
        }
        if (cluster_is_eoc(next)) {
            return i + 1U == count ? 0 : -1;
        }
        cluster = next;
    }
    return -1;
}

int fat32_mount(fat32_fs_t *fs, fat32_read_sector_fn_t read_sector_cb,
                void *context) {
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t reserved;
    uint32_t fat_count;
    uint32_t root_entries;
    uint32_t total_sectors;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    uint64_t fat_sectors;
    uint64_t data_start;
    uint64_t cluster_count;
    uint64_t fat_entries;

    if (fs == 0 || read_sector_cb == 0) {
        return -1;
    }

    fs->read_sector = read_sector_cb;
    fs->write_sector = 0;
    fs->context = context;
    fs->mounted = 0;
    fs->cluster_count = 0;
    fs->max_cluster = 0;

    if (read_sector(fs, 0) != 0) {
        return -1;
    }

    bytes_per_sector = le16(&fs->sector[11]);
    sectors_per_cluster = fs->sector[13];
    reserved = le16(&fs->sector[14]);
    fat_count = fs->sector[16];
    root_entries = le16(&fs->sector[17]);
    total_sectors = le16(&fs->sector[19]);
    if (total_sectors == 0) {
        total_sectors = le32(&fs->sector[32]);
    }
    sectors_per_fat = le32(&fs->sector[36]);
    root_cluster = le32(&fs->sector[44]) & FAT32_CLUSTER_MASK;

    if (bytes_per_sector != FAT32_SECTOR_SIZE ||
        sectors_per_cluster == 0 || sectors_per_cluster > 128U ||
        (sectors_per_cluster & (sectors_per_cluster - 1U)) != 0 ||
        reserved == 0 || fat_count == 0 || root_entries != 0 ||
        total_sectors == 0 || sectors_per_fat == 0) {
        return -1;
    }

    fat_sectors = (uint64_t)fat_count * sectors_per_fat;
    data_start = (uint64_t)reserved + fat_sectors;
    if (data_start >= total_sectors || data_start > UINT32_MAX) {
        return -1;
    }

    cluster_count = ((uint64_t)total_sectors - data_start) /
                    sectors_per_cluster;
    fat_entries = (uint64_t)sectors_per_fat * FAT32_SECTOR_SIZE / 4U;
    if (cluster_count == 0 || cluster_count > UINT32_MAX - 2U ||
        cluster_count + 2U > fat_entries ||
        root_cluster < 2U || root_cluster >= cluster_count + 2U) {
        return -1;
    }

    fs->bytes_per_sector = bytes_per_sector;
    fs->sectors_per_cluster = sectors_per_cluster;
    fs->reserved_sectors = reserved;
    fs->fat_count = fat_count;
    fs->sectors_per_fat = sectors_per_fat;
    fs->total_sectors = total_sectors;
    fs->fat_start_lba = reserved;
    fs->data_start_lba = (uint32_t)data_start;
    fs->root_cluster = root_cluster;
    fs->cluster_count = (uint32_t)cluster_count;
    fs->max_cluster = (uint32_t)cluster_count + 2U;
    fs->mounted = 1;

    {
        uint32_t ignored;
        if (cluster_lba(fs, root_cluster, &ignored) != 0) {
            fs->mounted = 0;
            return -1;
        }
    }
    return 0;
}

void fat32_set_write_sector(fat32_fs_t *fs,
                            fat32_write_sector_fn_t write_sector_cb) {
    if (fs != 0) {
        fs->write_sector = write_sector_cb;
    }
}

int fat32_open_root(fat32_fs_t *fs, const char *name, fat32_file_t *file) {
    uint8_t wanted[11];
    uint32_t cluster;

    if (fs == 0 || fs->mounted == 0 || file == 0 ||
        make_short_name(name, wanted) != 0) {
        return -1;
    }

    cluster = fs->root_cluster;
    for (uint32_t steps = 0; steps < fs->cluster_count; steps++) {
        uint32_t base_lba;

        if (cluster_lba(fs, cluster, &base_lba) != 0) {
            return -1;
        }
        for (uint32_t sector = 0; sector < fs->sectors_per_cluster; sector++) {
            uint32_t lba = base_lba + sector;

            if (read_sector(fs, lba) != 0) {
                return -1;
            }
            for (uint32_t offset = 0; offset < FAT32_SECTOR_SIZE;
                 offset += 32U) {
                const uint8_t *entry = &fs->sector[offset];
                uint8_t attr = entry[11];
                uint32_t first_cluster;

                if (entry[0] == 0x00U) {
                    return -1;
                }
                if (entry[0] == 0xe5U || attr == FAT32_ATTR_LONG_NAME ||
                    (attr & (FAT32_ATTR_DIRECTORY |
                             FAT32_ATTR_VOLUME_ID)) != 0 ||
                    !short_name_equals(entry, wanted)) {
                    continue;
                }

                first_cluster = ((uint32_t)le16(&entry[20]) << 16) |
                                le16(&entry[26]);
                file->first_cluster = first_cluster;
                file->dir_lba = lba;
                file->dir_offset = offset;
                file->capacity = 0;
                file->size = le32(&entry[28]);

                if (first_cluster < 2U) {
                    return file->size == 0 ? 0 : -1;
                }
                if (file_chain_capacity(fs, first_cluster,
                                        &file->capacity, 0) != 0 ||
                    file->size > file->capacity) {
                    return -1;
                }
                return 0;
            }
        }

        {
            uint32_t next;
            if (read_chain_next(fs, cluster, &next) != 0) {
                return -1;
            }
            if (cluster_is_eoc(next)) {
                return -1;
            }
            cluster = next;
        }
    }
    return -1;
}

int fat32_list_root_at(fat32_fs_t *fs, uint64_t offset, uint8_t *buffer,
                       uint64_t capacity, uint64_t *bytes_written) {
    fat32_list_output_t out = {
        .buffer = buffer,
        .capacity = capacity,
        .offset = offset,
        .position = 0,
        .written = 0,
    };
    uint32_t cluster;

    if (bytes_written != 0) {
        *bytes_written = 0;
    }
    if (fs == 0 || fs->mounted == 0 || buffer == 0 ||
        bytes_written == 0) {
        return -1;
    }

    cluster = fs->root_cluster;
    for (uint32_t steps = 0; steps < fs->cluster_count; steps++) {
        uint32_t base_lba;

        if (cluster_lba(fs, cluster, &base_lba) != 0) {
            return -1;
        }
        for (uint32_t sector = 0; sector < fs->sectors_per_cluster; sector++) {
            if (read_sector(fs, base_lba + sector) != 0) {
                return -1;
            }
            for (uint32_t pos = 0; pos < FAT32_SECTOR_SIZE; pos += 32U) {
                const uint8_t *entry = &fs->sector[pos];
                uint8_t attr = entry[11];

                if (entry[0] == 0x00U) {
                    *bytes_written = out.written;
                    return 0;
                }
                if (entry[0] == 0xe5U || attr == FAT32_ATTR_LONG_NAME ||
                    (attr & FAT32_ATTR_VOLUME_ID) != 0) {
                    continue;
                }
                if (append_short_name(entry, attr, &out) != 0) {
                    *bytes_written = out.written;
                    return 0;
                }
            }
        }

        {
            uint32_t next;
            if (read_chain_next(fs, cluster, &next) != 0) {
                return -1;
            }
            if (cluster_is_eoc(next)) {
                *bytes_written = out.written;
                return 0;
            }
            cluster = next;
        }
    }
    return -1;
}

int fat32_list_root(fat32_fs_t *fs, uint8_t *buffer, uint64_t capacity,
                    uint64_t *bytes_written) {
    return fat32_list_root_at(fs, 0, buffer, capacity, bytes_written);
}

int fat32_read(fat32_fs_t *fs, const fat32_file_t *file, uint64_t offset,
               uint8_t *buffer, uint64_t capacity, uint64_t *bytes_read) {
    uint64_t cluster_size;
    uint64_t remaining;
    uint64_t copied = 0;
    uint64_t skip;
    uint32_t cluster;
    uint32_t steps = 0;

    if (bytes_read != 0) {
        *bytes_read = 0;
    }
    if (fs == 0 || fs->mounted == 0 || file == 0 || buffer == 0 ||
        bytes_read == 0 || offset > file->size ||
        file->size > file->capacity) {
        return -1;
    }

    remaining = file->size - offset;
    if (remaining > capacity) {
        remaining = capacity;
    }
    if (remaining == 0) {
        return 0;
    }
    if (!cluster_is_data(fs, file->first_cluster)) {
        return -1;
    }

    cluster = file->first_cluster;
    cluster_size = (uint64_t)fs->sectors_per_cluster * FAT32_SECTOR_SIZE;
    skip = offset;

    while (skip >= cluster_size) {
        uint32_t next;
        if (++steps >= fs->cluster_count ||
            read_chain_next(fs, cluster, &next) != 0 ||
            cluster_is_eoc(next)) {
            return -1;
        }
        cluster = next;
        skip -= cluster_size;
    }

    while (remaining != 0) {
        uint64_t cluster_offset = skip;
        uint32_t base_lba;

        if (cluster_lba(fs, cluster, &base_lba) != 0) {
            return -1;
        }
        for (uint32_t sector = 0;
             sector < fs->sectors_per_cluster && remaining != 0; sector++) {
            uint64_t count;

            if (cluster_offset >= FAT32_SECTOR_SIZE) {
                cluster_offset -= FAT32_SECTOR_SIZE;
                continue;
            }
            if (read_sector(fs, base_lba + sector) != 0) {
                return -1;
            }
            count = FAT32_SECTOR_SIZE - cluster_offset;
            if (count > remaining) {
                count = remaining;
            }
            for (uint64_t i = 0; i < count; i++) {
                buffer[copied + i] = fs->sector[cluster_offset + i];
            }
            copied += count;
            remaining -= count;
            cluster_offset = 0;
        }

        skip = 0;
        if (remaining != 0) {
            uint32_t next;
            if (++steps >= fs->cluster_count ||
                read_chain_next(fs, cluster, &next) != 0 ||
                cluster_is_eoc(next)) {
                return -1;
            }
            cluster = next;
        }
    }

    *bytes_read = copied;
    return 0;
}

int fat32_write(fat32_fs_t *fs, fat32_file_t *file, uint64_t offset,
                const uint8_t *buffer, uint64_t size,
                uint64_t *bytes_written) {
    uint64_t cluster_size;
    uint64_t remaining;
    uint64_t copied = 0;
    uint64_t skip;
    uint32_t cluster;
    uint32_t steps = 0;
    uint32_t actual_capacity = 0;

    if (bytes_written != 0) {
        *bytes_written = 0;
    }
    if (fs == 0 || fs->mounted == 0 || fs->write_sector == 0 ||
        file == 0 || buffer == 0 || bytes_written == 0 ||
        offset > file->size || file->dir_lba >= fs->total_sectors ||
        file->dir_offset > FAT32_SECTOR_SIZE - 32U ||
        size > UINT32_MAX || offset > UINT32_MAX - size) {
        return -1;
    }

    if (size == 0) {
        if (read_sector(fs, file->dir_lba) != 0) {
            return -1;
        }
        put_le32(&fs->sector[file->dir_offset + 28U], (uint32_t)offset);
        if (write_sector(fs, file->dir_lba, fs->sector) != 0) {
            return -1;
        }
        file->size = (uint32_t)offset;
        return 0;
    }

    cluster_size = (uint64_t)fs->sectors_per_cluster * FAT32_SECTOR_SIZE;
    if (file->first_cluster >= 2U) {
        if (file_chain_capacity(fs, file->first_cluster,
                                &actual_capacity, 0) != 0 ||
            file->size > actual_capacity) {
            return -1;
        }
        file->capacity = actual_capacity;
    } else {
        uint32_t head;
        uint32_t base_lba;

        if (file->size != 0) {
            return -1;
        }
        head = allocate_cluster(fs, 0);
        if (head == 0 || cluster_lba(fs, head, &base_lba) != 0) {
            return -1;
        }
        for (uint32_t sector = 0; sector < fs->sectors_per_cluster; sector++) {
            for (uint32_t i = 0; i < FAT32_SECTOR_SIZE; i++) {
                fs->write_sector_buffer[i] = 0;
            }
            if (write_sector(fs, base_lba + sector,
                             fs->write_sector_buffer) != 0) {
                (void)write_fat_entry(fs, head, 0);
                return -1;
            }
        }
        if (read_sector(fs, file->dir_lba) != 0) {
            (void)write_fat_entry(fs, head, 0);
            return -1;
        }
        put_le16(&fs->sector[file->dir_offset + 20U],
                 (uint16_t)(head >> 16));
        put_le16(&fs->sector[file->dir_offset + 26U],
                 (uint16_t)head);
        if (write_sector(fs, file->dir_lba, fs->sector) != 0) {
            (void)write_fat_entry(fs, head, 0);
            return -1;
        }
        file->first_cluster = head;
        file->capacity = (uint32_t)cluster_size;
    }

    cluster = file->first_cluster;
    skip = offset;
    remaining = size;

    while (skip >= cluster_size) {
        if (++steps >= fs->cluster_count ||
            advance_or_allocate(fs, file, cluster_size, &cluster) != 0) {
            return -1;
        }
        skip -= cluster_size;
    }

    while (remaining != 0) {
        uint64_t cluster_offset = skip;
        uint32_t base_lba;

        if (cluster_lba(fs, cluster, &base_lba) != 0) {
            return -1;
        }
        for (uint32_t sector = 0;
             sector < fs->sectors_per_cluster && remaining != 0; sector++) {
            uint64_t count;

            if (cluster_offset >= FAT32_SECTOR_SIZE) {
                cluster_offset -= FAT32_SECTOR_SIZE;
                continue;
            }
            if (read_sector(fs, base_lba + sector) != 0) {
                return -1;
            }
            for (uint32_t i = 0; i < FAT32_SECTOR_SIZE; i++) {
                fs->write_sector_buffer[i] = fs->sector[i];
            }
            count = FAT32_SECTOR_SIZE - cluster_offset;
            if (count > remaining) {
                count = remaining;
            }
            for (uint64_t i = 0; i < count; i++) {
                fs->write_sector_buffer[cluster_offset + i] =
                    buffer[copied + i];
            }
            if (write_sector(fs, base_lba + sector,
                             fs->write_sector_buffer) != 0) {
                return -1;
            }
            copied += count;
            remaining -= count;
            cluster_offset = 0;
        }

        skip = 0;
        if (remaining != 0) {
            if (++steps >= fs->cluster_count ||
                advance_or_allocate(fs, file, cluster_size, &cluster) != 0) {
                return -1;
            }
        }
    }

    if (read_sector(fs, file->dir_lba) != 0) {
        return -1;
    }
    put_le32(&fs->sector[file->dir_offset + 28U],
             (uint32_t)(offset + copied));
    if (write_sector(fs, file->dir_lba, fs->sector) != 0) {
        return -1;
    }

    file->size = (uint32_t)(offset + copied);
    *bytes_written = copied;
    return 0;
}

static int find_free_dir_entry(fat32_fs_t *fs, uint32_t *out_lba,
                               uint32_t *out_offset) {
    uint32_t cluster = fs->root_cluster;

    if (out_lba == 0 || out_offset == 0) {
        return -1;
    }

    for (uint32_t steps = 0; steps < fs->cluster_count; steps++) {
        uint32_t base_lba;

        if (cluster_lba(fs, cluster, &base_lba) != 0) {
            return -1;
        }
        for (uint32_t sector = 0; sector < fs->sectors_per_cluster; sector++) {
            if (read_sector(fs, base_lba + sector) != 0) {
                return -1;
            }
            for (uint32_t offset = 0; offset < FAT32_SECTOR_SIZE;
                 offset += 32U) {
                if (fs->sector[offset] == 0x00U ||
                    fs->sector[offset] == 0xe5U) {
                    *out_lba = base_lba + sector;
                    *out_offset = offset;
                    return 0;
                }
            }
        }

        {
            uint32_t next;
            if (read_chain_next(fs, cluster, &next) != 0 ||
                cluster_is_eoc(next)) {
                return -1;
            }
            cluster = next;
        }
    }
    return -1;
}

static void write_dir_entry(uint8_t *sector, uint32_t offset,
                            const uint8_t name[11], uint8_t attr,
                            uint32_t first_cluster, uint32_t size) {
    uint8_t *entry = &sector[offset];

    for (uint32_t i = 0; i < 32U; i++) {
        entry[i] = 0;
    }
    for (uint32_t i = 0; i < 11U; i++) {
        entry[i] = name[i];
    }
    entry[11] = attr;
    put_le16(&entry[20], (uint16_t)(first_cluster >> 16));
    put_le16(&entry[26], (uint16_t)first_cluster);
    put_le32(&entry[28], size);
}

int fat32_create(fat32_fs_t *fs, const char *name, fat32_file_t *file) {
    uint8_t short_name[11];
    uint32_t dir_lba;
    uint32_t dir_offset;
    uint32_t cluster;
    uint32_t base_lba;

    if (fs == 0 || fs->mounted == 0 || fs->write_sector == 0 ||
        file == 0 || make_short_name(name, short_name) != 0 ||
        fat32_open_root(fs, name, file) == 0 ||
        find_free_dir_entry(fs, &dir_lba, &dir_offset) != 0) {
        return -1;
    }

    cluster = allocate_cluster(fs, 0);
    if (cluster == 0 || cluster_lba(fs, cluster, &base_lba) != 0) {
        return -1;
    }
    for (uint32_t i = 0; i < FAT32_SECTOR_SIZE; i++) {
        fs->write_sector_buffer[i] = 0;
    }
    for (uint32_t sector = 0; sector < fs->sectors_per_cluster; sector++) {
        if (write_sector(fs, base_lba + sector,
                         fs->write_sector_buffer) != 0) {
            (void)write_fat_entry(fs, cluster, 0);
            return -1;
        }
    }

    if (read_sector(fs, dir_lba) != 0) {
        (void)write_fat_entry(fs, cluster, 0);
        return -1;
    }
    write_dir_entry(fs->sector, dir_offset, short_name, 0x20U, cluster, 0);
    if (write_sector(fs, dir_lba, fs->sector) != 0) {
        (void)write_fat_entry(fs, cluster, 0);
        return -1;
    }

    file->first_cluster = cluster;
    file->dir_lba = dir_lba;
    file->dir_offset = dir_offset;
    file->capacity = fs->sectors_per_cluster * FAT32_SECTOR_SIZE;
    file->size = 0;
    return 0;
}

int fat32_delete(fat32_fs_t *fs, const char *name) {
    fat32_file_t file;
    uint8_t short_name[11];

    if (fs == 0 || fs->mounted == 0 || fs->write_sector == 0 ||
        make_short_name(name, short_name) != 0 ||
        fat32_open_root(fs, name, &file) != 0) {
        return -1;
    }

    /* Remove the directory reference first; an I/O failure can leak clusters,
     * but cannot leave a live entry pointing at a partially freed chain. */
    if (read_sector(fs, file.dir_lba) != 0) {
        return -1;
    }
    fs->sector[file.dir_offset] = 0xe5U;
    if (write_sector(fs, file.dir_lba, fs->sector) != 0) {
        return -1;
    }

    return file.first_cluster < 2U
               ? 0
               : free_cluster_chain(fs, file.first_cluster);
}

int fat32_rename(fat32_fs_t *fs, const char *old_name,
                 const char *new_name) {
    fat32_file_t file;
    fat32_file_t existing;
    uint8_t new_short[11];

    if (fs == 0 || fs->mounted == 0 || fs->write_sector == 0 ||
        make_short_name(new_name, new_short) != 0 ||
        fat32_open_root(fs, old_name, &file) != 0 ||
        fat32_open_root(fs, new_name, &existing) == 0 ||
        read_sector(fs, file.dir_lba) != 0) {
        return -1;
    }

    for (uint32_t i = 0; i < 11U; i++) {
        fs->sector[file.dir_offset + i] = new_short[i];
    }
    return write_sector(fs, file.dir_lba, fs->sector);
}
