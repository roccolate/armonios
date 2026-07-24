#!/usr/bin/env python3

from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    path.write_text(text.replace(old, new, 1))


vfs = Path("kernel/vfs.c")
replace_once(
    vfs,
    """    mount->ops.open = 0;\n    mount->ops.list = 0;\n""",
    """    mount->ops.open = 0;\n    mount->ops.read_path = 0;\n    mount->ops.list = 0;\n""",
)
replace_once(
    vfs,
    """        (ops->open == 0 && ops->list == 0 && ops->stat_path == 0 &&\n         ops->list_path == 0 && ops->metadata_path == 0 &&\n""",
    """        (ops->open == 0 && ops->read_path == 0 && ops->list == 0 &&\n         ops->stat_path == 0 && ops->list_path == 0 &&\n         ops->metadata_path == 0 &&\n""",
)
replace_once(
    vfs,
    """int vfs_read(const char *path, uint64_t offset, uint8_t *buffer,\n             uint64_t capacity, uint64_t *bytes_read) {\n    const vfs_node_t *node = vfs_find(path);\n    uint64_t size;\n    int status;\n\n    if (bytes_read != 0) {\n        *bytes_read = 0;\n    }\n\n    if (node == 0 || node->read == 0 || buffer == 0 || bytes_read == 0 ||\n        vfs_node_size(node, &size) != 0 || offset > size) {\n        return -1;\n    }\n\n    status = node->read(node->context, offset, buffer, capacity, bytes_read);\n    if (status != 0 ||\n        !vfs_read_result_valid(offset, size, capacity, *bytes_read)) {\n        *bytes_read = 0;\n        return status != 0 ? status : -1;\n    }\n\n    return 0;\n}\n""",
    """int vfs_read(const char *path, uint64_t offset, uint8_t *buffer,\n             uint64_t capacity, uint64_t *bytes_read) {\n    char canonical[VFS_MAX_PATH];\n    const vfs_node_t *node;\n    vfs_mount_t *mount;\n    vfs_metadata_t metadata;\n    uint64_t size;\n    int status;\n\n    if (bytes_read != 0) {\n        *bytes_read = 0;\n    }\n\n    if (buffer == 0 || bytes_read == 0 ||\n        vfs_normalize_path(path, canonical) != 0) {\n        return -1;\n    }\n\n    node = vfs_find_canonical(canonical);\n    if (node != 0) {\n        if (node->read == 0 || vfs_node_size(node, &size) != 0 ||\n            offset > size) {\n            return -1;\n        }\n        status = node->read(node->context, offset, buffer, capacity,\n                            bytes_read);\n    } else {\n        mount = vfs_find_mount_exact(canonical);\n        if (mount == 0) {\n            mount = vfs_find_mount_for_path(canonical);\n        }\n        if (mount == 0 || mount->ops.read_path == 0 ||\n            vfs_metadata(canonical, &metadata) != 0 ||\n            metadata.type != VFS_FILE_TYPE_REGULAR ||\n            offset > metadata.size) {\n            return -1;\n        }\n\n        size = metadata.size;\n        status = mount->ops.read_path(mount->context, canonical, offset,\n                                      buffer, capacity, bytes_read);\n    }\n\n    if (status != 0 ||\n        !vfs_read_result_valid(offset, size, capacity, *bytes_read)) {\n        *bytes_read = 0;\n        return status != 0 ? status : -1;\n    }\n\n    return 0;\n}\n""",
)

fat = Path("kernel/fat32_vfs.c")
replace_once(
    fat,
    """static int fat32_vfs_read(void *context, uint64_t offset, uint8_t *buffer,\n                          uint64_t capacity, uint64_t *bytes_read) {\n    fat32_vfs_file_t *mounted = (fat32_vfs_file_t *)context;\n\n    if (mounted == 0 || mounted->fs == 0) {\n        return -1;\n    }\n    return fat32_read(mounted->fs, &mounted->file, offset, buffer, capacity,\n                      bytes_read);\n}\n\n""",
    """static int fat32_vfs_read(void *context, uint64_t offset, uint8_t *buffer,\n                          uint64_t capacity, uint64_t *bytes_read) {\n    fat32_vfs_file_t *mounted = (fat32_vfs_file_t *)context;\n\n    if (mounted == 0 || mounted->fs == 0) {\n        return -1;\n    }\n    return fat32_read(mounted->fs, &mounted->file, offset, buffer, capacity,\n                      bytes_read);\n}\n\nstatic int fat32_vfs_read_path(void *context, const char *path,\n                               uint64_t offset, uint8_t *buffer,\n                               uint64_t capacity, uint64_t *bytes_read) {\n    char relative[VFS_MAX_PATH];\n    fat32_path_info_t info;\n    fat32_file_t file;\n    fat32_fs_t *fs = fat32_vfs_fs(context);\n\n    if (bytes_read != 0) {\n        *bytes_read = 0;\n    }\n    if (fs == 0 || buffer == 0 || bytes_read == 0 ||\n        fat32_vfs_relative_path(path, relative) != 0 ||\n        relative[0] == '\\0' || fat32_lookup_path(fs, relative, &info) != 0 ||\n        (info.attributes & FAT32_ATTR_DIRECTORY) != 0U || offset > info.size ||\n        fat32_open_path(fs, relative, &file) != 0) {\n        return -1;\n    }\n\n    return fat32_read(fs, &file, offset, buffer, capacity, bytes_read);\n}\n\n""",
)
replace_once(
    fat,
    """static const vfs_mount_ops_t g_fat32_vfs_ops = {\n    .open = fat32_vfs_open_path,\n    .list = fat32_vfs_list_root,\n""",
    """static const vfs_mount_ops_t g_fat32_vfs_ops = {\n    .open = fat32_vfs_open_path,\n    .read_path = fat32_vfs_read_path,\n    .list = fat32_vfs_list_root,\n""",
)
