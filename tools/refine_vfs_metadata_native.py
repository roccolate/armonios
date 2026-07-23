from pathlib import Path


def replace_exact(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"missing guarded text in {path}: {old[:100]!r}")
    p.write_text(text.replace(old, new, 1))


def insert_before(path: str, marker: str, addition: str) -> None:
    p = Path(path)
    text = p.read_text()
    if marker not in text:
        raise SystemExit(f"missing insertion marker in {path}: {marker[:100]!r}")
    p.write_text(text.replace(marker, addition + marker, 1))


replace_exact(
    "kernel/vfs.c",
    "    mount->ops.stat_path = 0;\n    mount->ops.list_path = 0;\n    mount->ops.unlink = 0;",
    "    mount->ops.stat_path = 0;\n    mount->ops.list_path = 0;\n    mount->ops.metadata_path = 0;\n    mount->ops.readdir_path = 0;\n    mount->ops.unlink = 0;",
)
replace_exact(
    "kernel/vfs.c",
    "        (ops->open == 0 && ops->list == 0 && ops->stat_path == 0 &&\n         ops->list_path == 0 && ops->unlink == 0 && ops->rename == 0) ||",
    "        (ops->open == 0 && ops->list == 0 && ops->stat_path == 0 &&\n         ops->list_path == 0 && ops->metadata_path == 0 &&\n         ops->readdir_path == 0 && ops->unlink == 0 && ops->rename == 0) ||",
)

structured_vfs = r'''
#define VFS_STRUCTURED_SCAN_LIMIT 4096U

static void vfs_metadata_clear(vfs_metadata_t *metadata) {
    metadata->size = 0;
    metadata->type = VFS_FILE_TYPE_UNKNOWN;
    metadata->attributes = 0;
}

static void vfs_dirent_clear(vfs_dirent_t *entry) {
    for (uint32_t i = 0; i < VFS_NAME_MAX; i++) {
        entry->name[i] = '\0';
    }
    vfs_metadata_clear(&entry->metadata);
}

static int vfs_metadata_type_valid(uint32_t type) {
    return type == VFS_FILE_TYPE_UNKNOWN ||
           type == VFS_FILE_TYPE_REGULAR ||
           type == VFS_FILE_TYPE_DIRECTORY;
}

static int vfs_dirent_name_valid(const char name[VFS_NAME_MAX]) {
    if (name == 0 || name[0] == '\0') {
        return 0;
    }
    for (uint32_t i = 0; i < VFS_NAME_MAX; i++) {
        if (name[i] == '\0') {
            return 1;
        }
    }
    return 0;
}

static int vfs_dirent_copy_name(char destination[VFS_NAME_MAX],
                                const char *source, uint32_t length) {
    if (destination == 0 || source == 0 || length == 0 ||
        length >= VFS_NAME_MAX) {
        return -1;
    }
    for (uint32_t i = 0; i < length; i++) {
        destination[i] = source[i];
    }
    destination[length] = '\0';
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

int vfs_metadata(const char *path, vfs_metadata_t *metadata) {
    char canonical[VFS_MAX_PATH];
    const vfs_node_t *node;
    vfs_mount_t *mount;
    vfs_stat_t legacy;
    uint8_t probe = 0;
    uint64_t listed = 0;
    int status;

    if (metadata == 0 || vfs_normalize_path(path, canonical) != 0) {
        return -1;
    }
    vfs_metadata_clear(metadata);

    node = vfs_find_canonical(canonical);
    if (node != 0) {
        if (vfs_node_size(node, &metadata->size) != 0) {
            return -1;
        }
        metadata->type = VFS_FILE_TYPE_REGULAR;
        return 0;
    }

    mount = vfs_find_mount_exact(canonical);
    if (mount == 0) {
        mount = vfs_find_mount_for_path(canonical);
    }
    if (mount != 0 && mount->ops.metadata_path != 0) {
        status = mount->ops.metadata_path(mount->context, canonical, metadata);
        if (status != 0 || !vfs_metadata_type_valid(metadata->type)) {
            vfs_metadata_clear(metadata);
            return status != 0 ? status : -1;
        }
        return 0;
    }

    if (mount != 0 && mount->ops.stat_path != 0) {
        if (mount->ops.stat_path(mount->context, canonical, &legacy) != 0) {
            return -1;
        }
        metadata->size = legacy.size;
        metadata->type =
            vfs_list_at(canonical, 0, &probe, 1, &listed) == 0
                ? VFS_FILE_TYPE_DIRECTORY
                : VFS_FILE_TYPE_REGULAR;
        return 0;
    }

    if (vfs_list_at(canonical, 0, &probe, 1, &listed) == 0) {
        metadata->type = VFS_FILE_TYPE_DIRECTORY;
        return 0;
    }
    return -1;
}

int vfs_readdir(const char *path, uint64_t start_index,
                vfs_dirent_t *entries, uint64_t max_entries,
                uint64_t *entries_written) {
    char canonical[VFS_MAX_PATH];
    vfs_mount_t *mount;
    char line[VFS_NAME_MAX + 1U];
    uint64_t byte_offset = 0;
    uint64_t logical_index = 0;
    uint64_t output = 0;
    uint32_t line_length = 0;
    int status;

    if (entries_written != 0) {
        *entries_written = 0;
    }
    if (entries == 0 || entries_written == 0 || max_entries == 0 ||
        max_entries > VFS_READDIR_MAX_ENTRIES ||
        vfs_normalize_path(path, canonical) != 0) {
        return -1;
    }

    mount = vfs_find_mount_exact(canonical);
    if (mount == 0) {
        mount = vfs_find_mount_for_path(canonical);
    }
    if (mount != 0 && mount->ops.readdir_path != 0) {
        status = mount->ops.readdir_path(mount->context, canonical, start_index,
                                         entries, max_entries, entries_written);
        if (status != 0 || *entries_written > max_entries) {
            *entries_written = 0;
            return status != 0 ? status : -1;
        }
        for (uint64_t i = 0; i < *entries_written; i++) {
            if (!vfs_dirent_name_valid(entries[i].name) ||
                !vfs_metadata_type_valid(entries[i].metadata.type)) {
                *entries_written = 0;
                return -1;
            }
        }
        return 0;
    }

    for (uint64_t scanned = 0; scanned < VFS_STRUCTURED_SCAN_LIMIT; scanned++) {
        uint8_t value = 0;
        uint64_t count = 0;

        if (vfs_list_at(canonical, byte_offset, &value, 1, &count) != 0) {
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
            if (line_length >= VFS_NAME_MAX) {
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

        vfs_dirent_t *entry = &entries[output];
        uint32_t name_length = line_length;
        uint8_t directory_hint = 0;
        char child[VFS_MAX_PATH];

        vfs_dirent_clear(entry);
        if (line[name_length - 1U] == '/') {
            name_length--;
            directory_hint = 1;
        }
        if (vfs_dirent_copy_name(entry->name, line, name_length) != 0) {
            return -1;
        }
        if (vfs_metadata_child_path(canonical, entry->name, child) == 0 &&
            vfs_metadata(child, &entry->metadata) == 0) {
            /* Native metadata resolved through the ordinary path. */
        } else {
            entry->metadata.type = directory_hint != 0
                                       ? VFS_FILE_TYPE_DIRECTORY
                                       : VFS_FILE_TYPE_REGULAR;
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

'''
replace_exact(
    "kernel/vfs.c",
    "int vfs_list(const char *path, uint8_t *buffer, uint64_t capacity,\n             uint64_t *bytes_written) {\n    return vfs_list_at(path, 0, buffer, capacity, bytes_written);\n}\n\n",
    "int vfs_list(const char *path, uint8_t *buffer, uint64_t capacity,\n             uint64_t *bytes_written) {\n    return vfs_list_at(path, 0, buffer, capacity, bytes_written);\n}\n\n" + structured_vfs,
)

replace_exact(
    "kernel/fat32_directory.c",
    "#define FAT32_ATTR_VOLUME_ID 0x08U\n#define FAT32_ATTR_LONG_NAME 0x0fU",
    "#define FAT32_ATTR_LONG_NAME 0x0fU",
)

fat_name_helper = r'''
static int dir_copy_name(const uint8_t entry[11],
                         char name[FAT32_SHORT_NAME_MAX]) {
    uint32_t base_end = 8U;
    uint32_t ext_end = 3U;
    uint32_t out = 0;

    while (base_end > 0U && entry[base_end - 1U] == ' ') {
        base_end--;
    }
    while (ext_end > 0U && entry[8U + ext_end - 1U] == ' ') {
        ext_end--;
    }
    if (base_end == 0U) {
        return -1;
    }
    for (uint32_t i = 0; i < base_end; i++) {
        if (out + 1U >= FAT32_SHORT_NAME_MAX) {
            return -1;
        }
        name[out++] = (char)entry[i];
    }
    if (ext_end > 0U) {
        if (out + 1U >= FAT32_SHORT_NAME_MAX) {
            return -1;
        }
        name[out++] = '.';
        for (uint32_t i = 0; i < ext_end; i++) {
            if (out + 1U >= FAT32_SHORT_NAME_MAX) {
                return -1;
            }
            name[out++] = (char)entry[8U + i];
        }
    }
    name[out] = '\0';
    return 0;
}

'''
insert_before(
    "kernel/fat32_directory.c",
    "static int dir_append_name(const uint8_t entry[11], uint8_t attr,\n",
    fat_name_helper,
)

fat_readdir = r'''
int fat32_readdir_path(fat32_fs_t *fs, const char *path,
                       uint64_t start_index, fat32_dirent_t *entries,
                       uint64_t max_entries, uint64_t *entries_written) {
    fat32_path_info_t directory;
    uint32_t cluster;
    uint32_t remaining;
    uint64_t logical_index = 0;
    uint64_t output = 0;

    if (entries_written != 0) {
        *entries_written = 0;
    }
    if (entries == 0 || entries_written == 0 || max_entries == 0 ||
        fat32_lookup_path(fs, path, &directory) != 0 ||
        (directory.attributes & FAT32_ATTR_DIRECTORY) == 0U) {
        return -1;
    }

    cluster = directory.first_cluster;
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
                fat32_path_info_t info;

                if (entry[0] == 0x00U) {
                    *entries_written = output;
                    return 0;
                }
                if (entry[0] == 0xe5U || attr == FAT32_ATTR_LONG_NAME ||
                    (attr & FAT32_ATTR_VOLUME_ID) != 0U ||
                    dir_is_dot_entry(entry)) {
                    continue;
                }
                if (logical_index++ < start_index) {
                    continue;
                }
                if (dir_fill_info(fs, entry, lba, entry_offset, &info) != 0 ||
                    dir_copy_name(entry, entries[output].name) != 0) {
                    return -1;
                }
                entries[output].size = info.size;
                entries[output].attributes = info.attributes;
                output++;
                if (output == max_entries) {
                    *entries_written = output;
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
    *entries_written = output;
    return 0;
}

'''
insert_before(
    "kernel/fat32_directory.c",
    "int fat32_list_path_at(fat32_fs_t *fs, const char *path, uint64_t offset,\n",
    fat_readdir,
)

fat_vfs_native = r'''
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

'''
insert_before(
    "kernel/fat32_vfs.c",
    "static int fat32_vfs_list_path(void *context, const char *path,\n",
    fat_vfs_native,
)
replace_exact(
    "kernel/fat32_vfs.c",
    "    .stat_path = fat32_vfs_stat_path,\n    .list_path = fat32_vfs_list_path,\n    .unlink = fat32_vfs_unlink_path,",
    "    .stat_path = fat32_vfs_stat_path,\n    .list_path = fat32_vfs_list_path,\n    .metadata_path = fat32_vfs_metadata_path,\n    .readdir_path = fat32_vfs_readdir_path,\n    .unlink = fat32_vfs_unlink_path,",
)

replace_exact(
    "programs/apps/files.c",
    "    char entries[ENTRY_CAP][NAME_CAP];\n    char list_buf[LIST_BUF_CAP];",
    "    char entries[ENTRY_CAP][NAME_CAP];\n    uint32_t entry_types[ENTRY_CAP];\n    uint64_t entry_sizes[ENTRY_CAP];\n    arm_dirent_v2_t dirents[ENTRY_CAP];\n    char list_buf[LIST_BUF_CAP];",
)
replace_exact(
    "programs/apps/files.c",
    "    for (int i = 0; i < ENTRY_CAP; i++) {\n        s->entries[i][0] = '\\0';\n    }",
    "    for (int i = 0; i < ENTRY_CAP; i++) {\n        s->entries[i][0] = '\\0';\n        s->entry_types[i] = ARM_FILE_TYPE_UNKNOWN;\n        s->entry_sizes[i] = 0;\n    }",
)

structured_files = r'''
static int load_structured_entries(files_state_t *s) {
    long count = kli_readdir_v2("/fat", 0, s->dirents, ENTRY_CAP);

    if (count < 0 || count > ENTRY_CAP) {
        return -1;
    }
    clear_entries(s);
    for (long i = 0; i < count; i++) {
        copy_cstr(s->entries[i], sizeof(s->entries[i]), s->dirents[i].name);
        s->entry_types[i] = s->dirents[i].type;
        s->entry_sizes[i] = s->dirents[i].size;
    }
    s->count = (int)count;
    return 0;
}

'''
insert_before(
    "programs/apps/files.c",
    "static void parse_list(files_state_t *s, long bytes) {\n",
    structured_files,
)
replace_exact(
    "programs/apps/files.c",
    r'''static void parse_list(files_state_t *s, long bytes) {
    int entry = 0;
    int col = 0;

    clear_entries(s);
    for (long i = 0; i < bytes && entry < ENTRY_CAP; i++) {
        char c = s->list_buf[i];
        if (c == '\n') {
            s->entries[entry][col] = '\0';
            if (col > 0) {
                entry++;
            }
            col = 0;
            continue;
        }
        if (col + 1 < NAME_CAP) {
            s->entries[entry][col++] = c;
        }
    }
    if (col > 0 && entry < ENTRY_CAP) {
        s->entries[entry][col] = '\0';
        entry++;
    }
    s->count = entry;
    if (s->selected >= s->count) {
        s->selected = s->count > 0 ? s->count - 1 : 0;
    }
}
''',
    r'''static void parse_list(files_state_t *s, long bytes) {
    int entry = 0;
    int col = 0;

    clear_entries(s);
    for (long i = 0; i < bytes && entry < ENTRY_CAP; i++) {
        char c = s->list_buf[i];
        if (c == '\n') {
            if (col > 0 && s->entries[entry][col - 1] == '/') {
                col--;
                s->entry_types[entry] = ARM_FILE_TYPE_DIRECTORY;
            } else {
                s->entry_types[entry] = ARM_FILE_TYPE_REGULAR;
            }
            s->entries[entry][col] = '\0';
            if (col > 0) {
                entry++;
            }
            col = 0;
            continue;
        }
        if (col + 1 < NAME_CAP) {
            s->entries[entry][col++] = c;
        }
    }
    if (col > 0 && entry < ENTRY_CAP) {
        if (s->entries[entry][col - 1] == '/') {
            col--;
            s->entry_types[entry] = ARM_FILE_TYPE_DIRECTORY;
        } else {
            s->entry_types[entry] = ARM_FILE_TYPE_REGULAR;
        }
        s->entries[entry][col] = '\0';
        entry++;
    }
    s->count = entry;
    if (s->selected >= s->count) {
        s->selected = s->count > 0 ? s->count - 1 : 0;
    }
}
''',
)
replace_exact(
    "programs/apps/files.c",
    r'''    long n = kli_readdir("/fat", s->list_buf, sizeof(s->list_buf));
    if (n < 0) {
        clear_entries(s);
        copy_cstr(s->status, sizeof(s->status), "FAT UNAVAILABLE");
        return;
    }
    parse_list(s, n);
    restore_selection(s, previous);
    copy_cstr(s->status, sizeof(s->status),
              s->count == 0 ? "NO FILES" : "FAT READY");
''',
    r'''    if (load_structured_entries(s) == 0) {
        restore_selection(s, previous);
        copy_cstr(s->status, sizeof(s->status),
                  s->count == 0 ? "NO FILES" : "FAT READY V2");
        return;
    }

    long n = kli_readdir("/fat", s->list_buf, sizeof(s->list_buf));
    if (n < 0) {
        clear_entries(s);
        copy_cstr(s->status, sizeof(s->status), "FAT UNAVAILABLE");
        return;
    }
    parse_list(s, n);
    restore_selection(s, previous);
    copy_cstr(s->status, sizeof(s->status),
              s->count == 0 ? "NO FILES" : "FAT LEGACY");
''',
)
replace_exact(
    "programs/apps/files.c",
    "            draw_text(s->wid, 16, y, COLOR_TEXT, s->entries[i]);",
    "            draw_text(s->wid, 16, y,\n                      s->entry_types[i] == ARM_FILE_TYPE_DIRECTORY\n                          ? COLOR_WARN\n                          : COLOR_TEXT,\n                      s->entries[i]);",
)
insert_before(
    "programs/apps/files.c",
    "static void open_selected(files_state_t *s) {\n",
    r'''static int selected_is_directory(files_state_t *s) {
    return s->count > 0 && s->selected >= 0 && s->selected < s->count &&
           s->entry_types[s->selected] == ARM_FILE_TYPE_DIRECTORY;
}

''',
)
replace_exact(
    "programs/apps/files.c",
    "    if (name == 0) {\n        copy_cstr(s->status, sizeof(s->status), \"NO SELECTION\");\n        return;\n    }\n\n    build_fat_path(s->editor_path, sizeof(s->editor_path), name);",
    "    if (name == 0) {\n        copy_cstr(s->status, sizeof(s->status), \"NO SELECTION\");\n        return;\n    }\n    if (selected_is_directory(s)) {\n        copy_cstr(s->status, sizeof(s->status), \"DIRECTORY (READ ONLY)\");\n        return;\n    }\n\n    build_fat_path(s->editor_path, sizeof(s->editor_path), name);",
)
replace_exact(
    "programs/apps/files.c",
    "    if (name == 0) {\n        copy_cstr(s->status, sizeof(s->status), \"NO SELECTION\");\n        return;\n    }\n    if (!valid_83_name(s->input)) {",
    "    if (name == 0) {\n        copy_cstr(s->status, sizeof(s->status), \"NO SELECTION\");\n        return;\n    }\n    if (selected_is_directory(s)) {\n        copy_cstr(s->status, sizeof(s->status), \"DIRECTORY RENAME LOCKED\");\n        return;\n    }\n    if (!valid_83_name(s->input)) {",
)
replace_exact(
    "programs/apps/files.c",
    "    if (name == 0) {\n        copy_cstr(s->status, sizeof(s->status), \"NO SELECTION\");\n        return;\n    }\n    build_fat_path(path, sizeof(path), name);",
    "    if (name == 0) {\n        copy_cstr(s->status, sizeof(s->status), \"NO SELECTION\");\n        return;\n    }\n    if (selected_is_directory(s)) {\n        copy_cstr(s->status, sizeof(s->status), \"DIRECTORY DELETE LOCKED\");\n        return;\n    }\n    build_fat_path(path, sizeof(path), name);",
)

replace_exact(
    "tests/test_fat32_directories.c",
    "    fat32_file_t file;\n    uint8_t buffer[32] = {0};",
    "    fat32_file_t file;\n    fat32_dirent_t entries[2];\n    uint8_t buffer[32] = {0};",
)
insert_before(
    "tests/test_fat32_directories.c",
    "    DIR_EQ(-1, fat32_list_path(&fs, \"docs/readme.txt\", buffer,\n",
    r'''    count = 0;
    DIR_EQ(0, fat32_readdir_path(&fs, "docs", 0, entries, 2, &count));
    DIR_EQ(2U, count);
    DIR_CHECK(directory_text_equal((const uint8_t *)entries[0].name,
                                   3, "SUB"));
    DIR_CHECK((entries[0].attributes & FAT32_ATTR_DIRECTORY) != 0U);
    DIR_CHECK(directory_text_equal((const uint8_t *)entries[1].name,
                                   10, "README.TXT"));
    DIR_EQ(4U, entries[1].size);
    DIR_CHECK((entries[1].attributes & FAT32_ATTR_ARCHIVE) != 0U);

''',
)
replace_exact(
    "tests/test_fat32_directories.c",
    "    vfs_stat_t stat;\n    uint8_t buffer[32] = {0};",
    "    vfs_stat_t stat;\n    vfs_metadata_t metadata;\n    vfs_dirent_t entries[2];\n    uint8_t buffer[32] = {0};",
)
insert_before(
    "tests/test_fat32_directories.c",
    "    fd = vfs_open(\"/fat/docs/sub/../sub/note.txt\");\n",
    r'''    DIR_EQ(0, vfs_metadata("/fat/docs/sub", &metadata));
    DIR_EQ(VFS_FILE_TYPE_DIRECTORY, metadata.type);
    DIR_EQ(0, vfs_metadata("/fat/docs/sub/note.txt", &metadata));
    DIR_EQ(VFS_FILE_TYPE_REGULAR, metadata.type);
    DIR_EQ(5U, metadata.size);
    DIR_CHECK((metadata.attributes & VFS_ATTRIBUTE_ARCHIVE) != 0U);

    count = 0;
    DIR_EQ(0, vfs_readdir("/fat/docs", 0, entries, 2, &count));
    DIR_EQ(2U, count);
    DIR_CHECK(directory_text_equal((const uint8_t *)entries[0].name,
                                   3, "SUB"));
    DIR_EQ(VFS_FILE_TYPE_DIRECTORY, entries[0].metadata.type);
    DIR_CHECK(directory_text_equal((const uint8_t *)entries[1].name,
                                   10, "README.TXT"));
    DIR_EQ(VFS_FILE_TYPE_REGULAR, entries[1].metadata.type);
    DIR_EQ(4U, entries[1].metadata.size);

''',
)

replace_exact(
    "docs/VFS_METADATA_ABI.md",
    "## Current adapter\n\nThe first kernel implementation adapts the existing VFS contracts:\n\n- `stat_v2` obtains size from `vfs_stat` and identifies directories through the\n  existing listing operation;\n- `readdir_v2` converts complete entries from the legacy listing stream and\n  obtains child sizes/types through `stat_v2`.\n\nThis keeps the ABI independent from FAT32. A later filesystem may add native\nstructured callbacks without changing syscall numbers or public layouts.",
    "## Current adapter\n\nThe VFS now owns filesystem-neutral `vfs_metadata_t` and `vfs_dirent_t` records.\nMounts may provide native path-aware metadata and bounded readdir callbacks. FAT32\nmaps short-name entries, file size, directory type, and FAT attributes directly\ninto those records.\n\nBootfs, tmpfs, and older list-only mounts retain a compatibility fallback that\nconverts their legacy stat/list operations into the same internal records. The\npublic syscall layer only versions and copies the internal result; it does not\nparse FAT-specific directory text. Files is the first EL0 consumer of\n`SYS_READDIR_V2`, with a legacy fallback while the old call remains supported.",
)
