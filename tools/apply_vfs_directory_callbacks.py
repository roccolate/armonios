#!/usr/bin/env python3
"""Apply path-aware stat/list mount callbacks to kernel/vfs.c.

Every replacement is exact and single-use. The script refuses partial or
repeated application so a branch cannot silently rewrite an unexpected VFS.
"""

from pathlib import Path

PATH = Path("kernel/vfs.c")
text = PATH.read_text()

if "mount->ops.stat_path = 0;" in text:
    raise SystemExit("VFS directory callback migration already applied")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    text = text.replace(old, new, 1)


replace_once(
    """    mount->ops.open = 0;
    mount->ops.list = 0;
    mount->ops.unlink = 0;
    mount->ops.rename = 0;
""",
    """    mount->ops.open = 0;
    mount->ops.list = 0;
    mount->ops.stat_path = 0;
    mount->ops.list_path = 0;
    mount->ops.unlink = 0;
    mount->ops.rename = 0;
""",
    "clear mount callbacks",
)

replace_once(
    """    if (vfs_normalize_path(path, canonical) != 0 || ops == 0 ||
        (ops->open == 0 && ops->list == 0 && ops->unlink == 0 &&
         ops->rename == 0) ||
        vfs_find_mount_exact(canonical) != 0) {
""",
    """    if (vfs_normalize_path(path, canonical) != 0 || ops == 0 ||
        (ops->open == 0 && ops->list == 0 && ops->stat_path == 0 &&
         ops->list_path == 0 && ops->unlink == 0 && ops->rename == 0) ||
        vfs_find_mount_exact(canonical) != 0) {
""",
    "mount callback validation",
)

replace_once(
    """int vfs_stat(const char *path, vfs_stat_t *stat) {
    const vfs_node_t *node = vfs_find(path);

    if (node == 0 || stat == 0) {
        return -1;
    }

    return vfs_node_size(node, &stat->size);
}
""",
    """int vfs_stat(const char *path, vfs_stat_t *stat) {
    char canonical[VFS_MAX_PATH];
    const vfs_node_t *node;
    vfs_mount_t *mount;

    if (stat == 0 || vfs_normalize_path(path, canonical) != 0) {
        return -1;
    }

    node = vfs_find_canonical(canonical);
    if (node != 0) {
        return vfs_node_size(node, &stat->size);
    }

    mount = vfs_find_mount_exact(canonical);
    if (mount == 0) {
        mount = vfs_find_mount_for_path(canonical);
    }
    if (mount == 0 || mount->ops.stat_path == 0) {
        return -1;
    }
    return mount->ops.stat_path(mount->context, canonical, stat);
}
""",
    "path-aware stat dispatch",
)

replace_once(
    """    mount = vfs_find_mount_exact(canonical);
    if (mount != 0 && mount->ops.list != 0) {
        status = mount->ops.list(mount->context, offset, buffer, capacity,
                                 bytes_written);
        if (status != 0 || *bytes_written > capacity) {
            *bytes_written = 0;
            return status != 0 ? status : -1;
        }
        return 0;
    }

    if (!kstreq(canonical, "/")) {
        return -1;
    }
""",
    """    mount = vfs_find_mount_exact(canonical);
    if (mount != 0 && mount->ops.list_path != 0) {
        status = mount->ops.list_path(mount->context, canonical, offset,
                                      buffer, capacity, bytes_written);
        if (status != 0 || *bytes_written > capacity) {
            *bytes_written = 0;
            return status != 0 ? status : -1;
        }
        return 0;
    }
    if (mount != 0 && mount->ops.list != 0) {
        status = mount->ops.list(mount->context, offset, buffer, capacity,
                                 bytes_written);
        if (status != 0 || *bytes_written > capacity) {
            *bytes_written = 0;
            return status != 0 ? status : -1;
        }
        return 0;
    }

    if (mount == 0) {
        mount = vfs_find_mount_for_path(canonical);
        if (mount != 0 && mount->ops.list_path != 0) {
            status = mount->ops.list_path(mount->context, canonical, offset,
                                          buffer, capacity, bytes_written);
            if (status != 0 || *bytes_written > capacity) {
                *bytes_written = 0;
                return status != 0 ? status : -1;
            }
            return 0;
        }
    }

    if (!kstreq(canonical, "/")) {
        return -1;
    }
""",
    "path-aware list dispatch",
)

PATH.write_text(text)
