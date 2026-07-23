#!/usr/bin/env python3
"""Apply the one-time canonical path migration to kernel/vfs.c.

Every replacement is exact and single-use. The script refuses partial,
ambiguous, or repeated application so the large VFS implementation is never
silently rewritten against an unexpected base.
"""

from pathlib import Path

PATH = Path("kernel/vfs.c")
text = PATH.read_text()

if "int vfs_normalize_path(" in text:
    raise SystemExit("VFS path migration already applied")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected one match, found {count}")
    text = text.replace(old, new, 1)


replace_once(
    '''static int vfs_path_is_mountable(const char *path) {
    uint32_t i = 0;

    if (path == 0 || path[0] != '/') {
        return 0;
    }

    while (path[i] != '\\0') {
        if (i + 1U >= VFS_MAX_PATH) {
            return 0;
        }
        i++;
    }

    return 1;
}

static int vfs_copy_path(char dest[VFS_MAX_PATH], const char *path) {
    uint32_t i = 0;

    if (dest == 0 || !vfs_path_is_mountable(path)) {
        return -1;
    }

    while (path[i] != '\\0') {
        dest[i] = path[i];
        i++;
    }
    dest[i] = '\\0';
    return 0;
}

static uint32_t vfs_path_length(const char *path) {
''',
    '''int vfs_normalize_path(const char *path,
                       char normalized[VFS_MAX_PATH]) {
    uint32_t input = 1U;
    uint32_t output = 1U;

    if (path == 0 || normalized == 0 || path[0] != '/') {
        return -1;
    }

    normalized[0] = '/';
    for (;;) {
        uint32_t component_start;
        uint32_t component_length;
        uint32_t required;

        while (input < VFS_MAX_PATH && path[input] == '/') {
            input++;
        }
        if (input >= VFS_MAX_PATH) {
            return -1;
        }
        if (path[input] == '\\0') {
            break;
        }

        component_start = input;
        while (input < VFS_MAX_PATH && path[input] != '/' &&
               path[input] != '\\0') {
            input++;
        }
        if (input >= VFS_MAX_PATH) {
            return -1;
        }
        component_length = input - component_start;

        if (component_length == 1U && path[component_start] == '.') {
            continue;
        }
        if (component_length == 2U && path[component_start] == '.' &&
            path[component_start + 1U] == '.') {
            if (output == 1U) {
                return -1;
            }
            while (output > 1U && normalized[output - 1U] != '/') {
                output--;
            }
            if (output > 1U) {
                output--;
            }
            continue;
        }

        required = component_length + (output > 1U ? 1U : 0U);
        if (output > (VFS_MAX_PATH - 1U) - required) {
            return -1;
        }
        if (output > 1U) {
            normalized[output++] = '/';
        }
        for (uint32_t i = 0; i < component_length; i++) {
            normalized[output++] = path[component_start + i];
        }
    }

    normalized[output] = '\\0';
    return 0;
}

static int vfs_copy_path(char dest[VFS_MAX_PATH], const char *path) {
    return vfs_normalize_path(path, dest);
}

static uint32_t vfs_path_length(const char *path) {
''',
    "path helper block",
)

replace_once(
    '''\nint vfs_mount_static(const vfs_node_t *nodes, uint32_t count) {\n''',
    '''\nstatic const vfs_node_t *vfs_find_canonical(const char *path) {
    if (path == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < g_vfs_node_count; i++) {
        if (g_vfs_nodes[i].path != 0 &&
            kstreq(g_vfs_nodes[i].path, path)) {
            return &g_vfs_nodes[i];
        }
    }

    return 0;
}

int vfs_mount_static(const vfs_node_t *nodes, uint32_t count) {
''',
    "canonical node lookup insertion",
)

replace_once(
    '''    for (uint32_t i = 0; i < count; i++) {
        const vfs_node_t *node = &nodes[i];

        if (!vfs_path_is_mountable(node->path) ||
            (node->read == 0 && node->write == 0) ||
            vfs_find(node->path) != 0) {
            return -1;
        }

        for (uint32_t j = 0; j < i; j++) {
            if (kstreq(nodes[j].path, node->path)) {
                return -1;
            }
        }
    }
''',
    '''    for (uint32_t i = 0; i < count; i++) {
        const vfs_node_t *node = &nodes[i];
        char normalized[VFS_MAX_PATH];

        if (vfs_normalize_path(node->path, normalized) != 0 ||
            (node->read == 0 && node->write == 0) ||
            vfs_find_canonical(normalized) != 0) {
            return -1;
        }

        for (uint32_t j = 0; j < i; j++) {
            char previous[VFS_MAX_PATH];

            if (vfs_normalize_path(nodes[j].path, previous) != 0 ||
                kstreq(previous, normalized)) {
                return -1;
            }
        }
    }
''',
    "static node validation",
)

replace_once(
    '''int vfs_unmount_static(const char *path) {
    if (path == 0 || path[0] == '\\0') {
        return -1;
    }

    for (uint32_t i = 0; i < g_vfs_node_count; i++) {
        if (kstreq(g_vfs_nodes[i].path, path)) {
            const vfs_node_t *node = &g_vfs_nodes[i];

            vfs_drop_open_files_for_node(node);
            vfs_clear_node(i);
            return 0;
        }
    }

    return -1;
}
''',
    '''int vfs_unmount_static(const char *path) {
    char canonical[VFS_MAX_PATH];

    if (vfs_normalize_path(path, canonical) != 0) {
        return -1;
    }

    for (uint32_t i = 0; i < g_vfs_node_count; i++) {
        if (g_vfs_nodes[i].path != 0 &&
            kstreq(g_vfs_nodes[i].path, canonical)) {
            const vfs_node_t *node = &g_vfs_nodes[i];

            vfs_drop_open_files_for_node(node);
            vfs_clear_node(i);
            return 0;
        }
    }

    return -1;
}
''',
    "static node unmount",
)

replace_once(
    '''int vfs_mount(const char *path, const vfs_mount_ops_t *ops, void *context) {
    if (!vfs_path_is_mountable(path) || ops == 0 ||
        (ops->open == 0 && ops->list == 0 && ops->unlink == 0 &&
         ops->rename == 0) ||
        vfs_find_mount_exact(path) != 0) {
        return -1;
    }

    for (uint32_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (g_mounts[i].used == 0) {
            if (vfs_copy_path(g_mounts[i].path, path) != 0) {
                return -1;
            }
            g_mounts[i].ops = *ops;
            g_mounts[i].context = context;
            g_mounts[i].used = 1;
            return 0;
        }
    }

    return -1;
}
''',
    '''int vfs_mount(const char *path, const vfs_mount_ops_t *ops, void *context) {
    char canonical[VFS_MAX_PATH];

    if (vfs_normalize_path(path, canonical) != 0 || ops == 0 ||
        (ops->open == 0 && ops->list == 0 && ops->unlink == 0 &&
         ops->rename == 0) ||
        vfs_find_mount_exact(canonical) != 0) {
        return -1;
    }

    for (uint32_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (g_mounts[i].used == 0) {
            if (vfs_copy_path(g_mounts[i].path, canonical) != 0) {
                return -1;
            }
            g_mounts[i].ops = *ops;
            g_mounts[i].context = context;
            g_mounts[i].used = 1;
            return 0;
        }
    }

    return -1;
}
''',
    "mount registration",
)

replace_once(
    '''const vfs_node_t *vfs_find(const char *path) {
    if (path == 0 || path[0] == '\\0') {
        return 0;
    }

    for (uint32_t i = 0; i < g_vfs_node_count; i++) {
        if (kstreq(g_vfs_nodes[i].path, path)) {
            return &g_vfs_nodes[i];
        }
    }

    return 0;
}
''',
    '''const vfs_node_t *vfs_find(const char *path) {
    char canonical[VFS_MAX_PATH];

    if (vfs_normalize_path(path, canonical) != 0) {
        return 0;
    }
    return vfs_find_canonical(canonical);
}
''',
    "public node lookup",
)

replace_once(
    '''int vfs_list_at(const char *path, uint64_t offset, uint8_t *buffer,
                uint64_t capacity, uint64_t *bytes_written) {
    vfs_mount_t *mount;
    uint64_t position = 0;
    uint64_t out = 0;
    int status;

    if (bytes_written != 0) {
        *bytes_written = 0;
    }

    if (path == 0 || buffer == 0 || bytes_written == 0 ||
        path[0] == '\\0') {
        return -1;
    }

    mount = vfs_find_mount_exact(path);
    if (mount != 0 && mount->ops.list != 0) {
        status = mount->ops.list(mount->context, offset, buffer, capacity,
                                 bytes_written);
        if (status != 0 || *bytes_written > capacity) {
            *bytes_written = 0;
            return status != 0 ? status : -1;
        }
        return 0;
    }

    if (!kstreq(path, "/")) {
        return -1;
    }

    for (uint32_t i = 0; i < g_vfs_node_count; i++) {
        const char *node_path = g_vfs_nodes[i].path;

        if (node_path == 0) {
            continue;
        }

        while (*node_path != '\\0') {
            if (vfs_list_emit_byte(offset, buffer, capacity, &position, &out,
                                   (uint8_t)*node_path) != 0) {
                *bytes_written = out;
                return 0;
            }
            node_path++;
        }

        if (vfs_list_emit_byte(offset, buffer, capacity, &position, &out,
                               (uint8_t)'\\n') != 0) {
            *bytes_written = out;
            return 0;
        }
    }

    *bytes_written = out;
    return 0;
}
''',
    '''int vfs_list_at(const char *path, uint64_t offset, uint8_t *buffer,
                uint64_t capacity, uint64_t *bytes_written) {
    char canonical[VFS_MAX_PATH];
    vfs_mount_t *mount;
    uint64_t position = 0;
    uint64_t out = 0;
    int status;

    if (bytes_written != 0) {
        *bytes_written = 0;
    }

    if (buffer == 0 || bytes_written == 0 ||
        vfs_normalize_path(path, canonical) != 0) {
        return -1;
    }

    mount = vfs_find_mount_exact(canonical);
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

    for (uint32_t i = 0; i < g_vfs_node_count; i++) {
        const char *node_path = g_vfs_nodes[i].path;

        if (node_path == 0) {
            continue;
        }

        while (*node_path != '\\0') {
            if (vfs_list_emit_byte(offset, buffer, capacity, &position, &out,
                                   (uint8_t)*node_path) != 0) {
                *bytes_written = out;
                return 0;
            }
            node_path++;
        }

        if (vfs_list_emit_byte(offset, buffer, capacity, &position, &out,
                               (uint8_t)'\\n') != 0) {
            *bytes_written = out;
            return 0;
        }
    }

    *bytes_written = out;
    return 0;
}
''',
    "directory listing",
)

replace_once(
    '''int vfs_open_flags(const char *path, uint32_t flags) {
    const vfs_node_t *node;
    vfs_mount_t *mount;
    uint32_t mode = vfs_open_access_mode(flags);
    uint32_t owner_pid;
    uint32_t local_fd;
    vfs_open_file_t *file;

    if (!vfs_open_flags_valid(flags)) {
        return -1;
    }

    node = vfs_find(path);
    if (node == 0) {
        mount = vfs_find_mount_for_path(path);
        if (mount == 0 || mount->ops.open == 0 ||
            mount->ops.open(mount->context, path, flags) != 0) {
            return -1;
        }
        node = vfs_find(path);
    }

    if (node == 0 ||
        ((mode == VFS_O_RDONLY || mode == VFS_O_RDWR) &&
         node->read == 0) ||
        ((mode == VFS_O_WRONLY || mode == VFS_O_RDWR) &&
         node->write == 0)) {
        return -1;
    }

    vfs_reap_dead_owners();
    owner_pid = vfs_current_owner_pid();
    if (vfs_find_free_local_fd(owner_pid, &local_fd) != 0) {
        return -1;
    }

    file = vfs_find_free_global_handle();
    if (file == 0) {
        return -1;
    }

    file->node = node;
    file->offset = 0;
    file->flags = mode;
    file->owner_pid = owner_pid;
    file->local_fd = local_fd;
    file->used = 1;
    return (int)local_fd;
}
''',
    '''int vfs_open_flags(const char *path, uint32_t flags) {
    char canonical[VFS_MAX_PATH];
    const vfs_node_t *node;
    vfs_mount_t *mount;
    uint32_t mode = vfs_open_access_mode(flags);
    uint32_t owner_pid;
    uint32_t local_fd;
    vfs_open_file_t *file;

    if (!vfs_open_flags_valid(flags) ||
        vfs_normalize_path(path, canonical) != 0) {
        return -1;
    }

    node = vfs_find_canonical(canonical);
    if (node == 0) {
        mount = vfs_find_mount_for_path(canonical);
        if (mount == 0 || mount->ops.open == 0 ||
            mount->ops.open(mount->context, canonical, flags) != 0) {
            return -1;
        }
        node = vfs_find_canonical(canonical);
    }

    if (node == 0 ||
        ((mode == VFS_O_RDONLY || mode == VFS_O_RDWR) &&
         node->read == 0) ||
        ((mode == VFS_O_WRONLY || mode == VFS_O_RDWR) &&
         node->write == 0)) {
        return -1;
    }

    vfs_reap_dead_owners();
    owner_pid = vfs_current_owner_pid();
    if (vfs_find_free_local_fd(owner_pid, &local_fd) != 0) {
        return -1;
    }

    file = vfs_find_free_global_handle();
    if (file == 0) {
        return -1;
    }

    file->node = node;
    file->offset = 0;
    file->flags = mode;
    file->owner_pid = owner_pid;
    file->local_fd = local_fd;
    file->used = 1;
    return (int)local_fd;
}
''',
    "open dispatch",
)

replace_once(
    '''int vfs_unlink(const char *path) {
    vfs_mount_t *mount = vfs_find_mount_for_path(path);

    if (mount == 0 || mount->ops.unlink == 0 ||
        mount->ops.unlink(mount->context, path) != 0) {
        return -1;
    }

    (void)vfs_unmount_static(path);
    return 0;
}

int vfs_rename(const char *old_path, const char *new_path) {
    vfs_mount_t *old_mount = vfs_find_mount_for_path(old_path);
    vfs_mount_t *new_mount = vfs_find_mount_for_path(new_path);

    if (old_mount == 0 || old_mount != new_mount ||
        old_mount->ops.rename == 0 ||
        old_mount->ops.rename(old_mount->context, old_path, new_path) != 0) {
        return -1;
    }

    (void)vfs_unmount_static(old_path);
    (void)vfs_unmount_static(new_path);
    return 0;
}
''',
    '''int vfs_unlink(const char *path) {
    char canonical[VFS_MAX_PATH];
    vfs_mount_t *mount;

    if (vfs_normalize_path(path, canonical) != 0) {
        return -1;
    }
    mount = vfs_find_mount_for_path(canonical);

    if (mount == 0 || mount->ops.unlink == 0 ||
        mount->ops.unlink(mount->context, canonical) != 0) {
        return -1;
    }

    (void)vfs_unmount_static(canonical);
    return 0;
}

int vfs_rename(const char *old_path, const char *new_path) {
    char canonical_old[VFS_MAX_PATH];
    char canonical_new[VFS_MAX_PATH];
    vfs_mount_t *old_mount;
    vfs_mount_t *new_mount;

    if (vfs_normalize_path(old_path, canonical_old) != 0 ||
        vfs_normalize_path(new_path, canonical_new) != 0) {
        return -1;
    }

    old_mount = vfs_find_mount_for_path(canonical_old);
    new_mount = vfs_find_mount_for_path(canonical_new);
    if (old_mount == 0 || old_mount != new_mount ||
        old_mount->ops.rename == 0 ||
        old_mount->ops.rename(old_mount->context, canonical_old,
                              canonical_new) != 0) {
        return -1;
    }

    (void)vfs_unmount_static(canonical_old);
    (void)vfs_unmount_static(canonical_new);
    return 0;
}
''',
    "mutation dispatch",
)

if "vfs_path_is_mountable" in text:
    raise SystemExit("legacy mountability helper remains")
if text.count("int vfs_normalize_path(") != 1:
    raise SystemExit("normalizer was not installed exactly once")
if "mount->ops.open(mount->context, canonical, flags)" not in text:
    raise SystemExit("open callback is not canonicalized")

PATH.write_text(text)
