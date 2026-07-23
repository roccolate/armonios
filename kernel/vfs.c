#include "kernel/vfs.h"

#include <stdint.h>

#include "kernel/kstring.h"
#include "kernel/process.h"

/*
 * Small in-kernel virtual filesystem.
 *
 * Static nodes hold ordinary files. Mounts only route operations for paths
 * that are not materialized yet, such as files discovered on a disk volume.
 * The global open-file array is kernel-private; each process sees local fd
 * numbers and owns its own offsets.
 */

static vfs_node_t g_vfs_nodes[VFS_MAX_NODES];
static char g_vfs_paths[VFS_MAX_NODES][VFS_MAX_PATH];
static uint32_t g_vfs_node_count;

typedef struct {
    char path[VFS_MAX_PATH];
    vfs_mount_ops_t ops;
    void *context;
    uint8_t used;
} vfs_mount_t;

static vfs_mount_t g_mounts[VFS_MAX_MOUNTS];

typedef struct {
    const vfs_node_t *node;
    uint64_t offset;
    uint32_t flags;
    uint32_t owner_pid;
    uint32_t local_fd;
    uint8_t used;
} vfs_open_file_t;

static vfs_open_file_t g_open_files[VFS_MAX_GLOBAL_OPEN_FILES];

static uint32_t vfs_current_owner_pid(void) {
    const process_t *process = process_current();

    return process != 0 ? process->pid : 0U;
}

static void vfs_clear_open_file(vfs_open_file_t *file) {
    if (file == 0) {
        return;
    }

    file->node = 0;
    file->offset = 0;
    file->flags = VFS_O_RDONLY;
    file->owner_pid = 0;
    file->local_fd = 0;
    file->used = 0;
}

static int vfs_owner_is_alive(uint32_t pid) {
    const process_t *current;
    const process_t *process;

    if (pid == 0U) {
        return 1;
    }

    current = process_current();
    if (current != 0 && current->pid == pid) {
        return current->state != PROCESS_UNUSED &&
               current->state != PROCESS_ZOMBIE;
    }

    process = process_find(pid);
    return process != 0 && process->state != PROCESS_UNUSED &&
           process->state != PROCESS_ZOMBIE;
}

static void vfs_reap_dead_owners(void) {
    for (uint32_t i = 0; i < VFS_MAX_GLOBAL_OPEN_FILES; i++) {
        if (g_open_files[i].used != 0 &&
            !vfs_owner_is_alive(g_open_files[i].owner_pid)) {
            vfs_clear_open_file(&g_open_files[i]);
        }
    }
}

static void vfs_clear_node(uint32_t index) {
    if (index >= VFS_MAX_NODES) {
        return;
    }

    g_vfs_nodes[index].path = 0;
    g_vfs_nodes[index].size = 0;
    g_vfs_nodes[index].read = 0;
    g_vfs_nodes[index].write = 0;
    g_vfs_nodes[index].stat = 0;
    g_vfs_nodes[index].context = 0;
    for (uint32_t j = 0; j < VFS_MAX_PATH; j++) {
        g_vfs_paths[index][j] = '\0';
    }
}

static void vfs_clear_mount(vfs_mount_t *mount) {
    if (mount == 0) {
        return;
    }

    for (uint32_t i = 0; i < VFS_MAX_PATH; i++) {
        mount->path[i] = '\0';
    }
    mount->ops.open = 0;
    mount->ops.list = 0;
    mount->ops.stat_path = 0;
    mount->ops.list_path = 0;
    mount->ops.unlink = 0;
    mount->ops.rename = 0;
    mount->context = 0;
    mount->used = 0;
}

static uint32_t vfs_free_node_count(void) {
    uint32_t free_count = 0;

    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        if (g_vfs_nodes[i].path == 0) {
            free_count++;
        }
    }

    return free_count;
}

static int vfs_find_free_node(uint32_t *index) {
    if (index == 0) {
        return -1;
    }

    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        if (g_vfs_nodes[i].path == 0) {
            *index = i;
            return 0;
        }
    }

    return -1;
}

static void vfs_drop_open_files_for_node(const vfs_node_t *node) {
    if (node == 0) {
        return;
    }

    for (uint32_t i = 0; i < VFS_MAX_GLOBAL_OPEN_FILES; i++) {
        if (g_open_files[i].used != 0 && g_open_files[i].node == node) {
            vfs_clear_open_file(&g_open_files[i]);
        }
    }
}

static int vfs_local_fd_in_use(uint32_t owner_pid, uint32_t local_fd) {
    for (uint32_t i = 0; i < VFS_MAX_GLOBAL_OPEN_FILES; i++) {
        if (g_open_files[i].used != 0 &&
            g_open_files[i].owner_pid == owner_pid &&
            g_open_files[i].local_fd == local_fd) {
            return 1;
        }
    }

    return 0;
}

static int vfs_find_free_local_fd(uint32_t owner_pid, uint32_t *local_fd) {
    if (local_fd == 0) {
        return -1;
    }

    for (uint32_t fd = 0; fd < VFS_MAX_OPEN_FILES; fd++) {
        if (!vfs_local_fd_in_use(owner_pid, fd)) {
            *local_fd = fd;
            return 0;
        }
    }

    return -1;
}

static vfs_open_file_t *vfs_find_free_global_handle(void) {
    for (uint32_t i = 0; i < VFS_MAX_GLOBAL_OPEN_FILES; i++) {
        if (g_open_files[i].used == 0) {
            return &g_open_files[i];
        }
    }

    return 0;
}

static vfs_open_file_t *vfs_fd_at(int fd) {
    uint32_t owner_pid;

    if (fd < 0 || fd >= (int)VFS_MAX_OPEN_FILES) {
        return 0;
    }

    vfs_reap_dead_owners();
    owner_pid = vfs_current_owner_pid();

    for (uint32_t i = 0; i < VFS_MAX_GLOBAL_OPEN_FILES; i++) {
        if (g_open_files[i].used != 0 &&
            g_open_files[i].node != 0 &&
            g_open_files[i].owner_pid == owner_pid &&
            g_open_files[i].local_fd == (uint32_t)fd) {
            return &g_open_files[i];
        }
    }

    return 0;
}

int vfs_normalize_path(const char *path,
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
        if (path[input] == '\0') {
            break;
        }

        component_start = input;
        while (input < VFS_MAX_PATH && path[input] != '/' &&
               path[input] != '\0') {
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

    normalized[output] = '\0';
    return 0;
}

static int vfs_copy_path(char dest[VFS_MAX_PATH], const char *path) {
    return vfs_normalize_path(path, dest);
}

static uint32_t vfs_path_length(const char *path) {
    uint32_t length = 0;

    if (path == 0) {
        return 0;
    }
    while (path[length] != '\0') {
        length++;
    }
    return length;
}

static int vfs_path_is_mount_child(const char *path, const char *mount_path) {
    uint32_t i = 0;

    if (path == 0 || mount_path == 0 || path[0] != '/' ||
        mount_path[0] != '/') {
        return 0;
    }

    if (mount_path[0] == '/' && mount_path[1] == '\0') {
        return path[1] != '\0';
    }

    while (mount_path[i] != '\0') {
        if (path[i] != mount_path[i]) {
            return 0;
        }
        i++;
    }

    return path[i] == '/' && path[i + 1U] != '\0';
}

static vfs_mount_t *vfs_find_mount_exact(const char *path) {
    for (uint32_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (g_mounts[i].used != 0 && kstreq(g_mounts[i].path, path)) {
            return &g_mounts[i];
        }
    }

    return 0;
}

static vfs_mount_t *vfs_find_mount_for_path(const char *path) {
    vfs_mount_t *best = 0;
    uint32_t best_length = 0;

    for (uint32_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        uint32_t length;

        if (g_mounts[i].used == 0 ||
            !vfs_path_is_mount_child(path, g_mounts[i].path)) {
            continue;
        }

        length = vfs_path_length(g_mounts[i].path);
        if (best == 0 || length > best_length) {
            best = &g_mounts[i];
            best_length = length;
        }
    }

    return best;
}

static int vfs_node_size(const vfs_node_t *node, uint64_t *size) {
    vfs_stat_t stat;

    if (node == 0 || size == 0) {
        return -1;
    }

    if (node->stat != 0) {
        if (node->stat(node->context, &stat) != 0) {
            return -1;
        }
        *size = stat.size;
    } else {
        *size = node->size;
    }

    return 0;
}

static int vfs_read_result_valid(uint64_t offset, uint64_t file_size,
                                 uint64_t capacity, uint64_t count) {
    if (offset > file_size || count > capacity) {
        return 0;
    }

    return count <= file_size - offset;
}

static int vfs_write_result_valid(uint64_t offset, uint64_t requested,
                                  uint64_t count) {
    if (count > requested || offset > UINT64_MAX - count) {
        return 0;
    }

    return 1;
}

static uint32_t vfs_open_access_mode(uint32_t flags) {
    return flags & VFS_O_ACCMODE;
}

static int vfs_open_flags_valid(uint32_t flags) {
    uint32_t mode = vfs_open_access_mode(flags);

    if ((flags & ~VFS_O_ALLOWED) != 0) {
        return 0;
    }

    return mode == VFS_O_RDONLY || mode == VFS_O_WRONLY ||
           mode == VFS_O_RDWR;
}

void vfs_reset(void) {
    for (uint32_t i = 0; i < VFS_MAX_NODES; i++) {
        vfs_clear_node(i);
    }
    for (uint32_t i = 0; i < VFS_MAX_GLOBAL_OPEN_FILES; i++) {
        vfs_clear_open_file(&g_open_files[i]);
    }
    for (uint32_t i = 0; i < VFS_MAX_MOUNTS; i++) {
        vfs_clear_mount(&g_mounts[i]);
    }
    g_vfs_node_count = 0;
}

static const vfs_node_t *vfs_find_canonical(const char *path) {
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
    if (nodes == 0 || count == 0 || count > vfs_free_node_count()) {
        return -1;
    }

    for (uint32_t i = 0; i < count; i++) {
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

    for (uint32_t i = 0; i < count; i++) {
        uint32_t index;

        if (vfs_find_free_node(&index) != 0 ||
            vfs_copy_path(g_vfs_paths[index], nodes[i].path) != 0) {
            return -1;
        }

        g_vfs_nodes[index].path = g_vfs_paths[index];
        g_vfs_nodes[index].size = nodes[i].size;
        g_vfs_nodes[index].read = nodes[i].read;
        g_vfs_nodes[index].write = nodes[i].write;
        g_vfs_nodes[index].stat = nodes[i].stat;
        g_vfs_nodes[index].context = nodes[i].context;
        if (index >= g_vfs_node_count) {
            g_vfs_node_count = index + 1U;
        }
    }

    return 0;
}

int vfs_unmount_static(const char *path) {
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

int vfs_mount(const char *path, const vfs_mount_ops_t *ops, void *context) {
    char canonical[VFS_MAX_PATH];

    if (vfs_normalize_path(path, canonical) != 0 || ops == 0 ||
        (ops->open == 0 && ops->list == 0 && ops->stat_path == 0 &&
         ops->list_path == 0 && ops->unlink == 0 && ops->rename == 0) ||
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

int vfs_mount_list(const char *path, vfs_list_fn_t list, void *context) {
    vfs_mount_ops_t ops = {
        .list = list,
    };

    if (list == 0) {
        return -1;
    }
    return vfs_mount(path, &ops, context);
}

const vfs_node_t *vfs_find(const char *path) {
    char canonical[VFS_MAX_PATH];

    if (vfs_normalize_path(path, canonical) != 0) {
        return 0;
    }
    return vfs_find_canonical(canonical);
}

const char *vfs_strip_prefix(const char *path, const char *prefix) {
    if (path == 0 || prefix == 0 || prefix[0] == '\0') {
        return 0;
    }

    while (*prefix != '\0') {
        if (*path != *prefix) {
            return 0;
        }
        path++;
        prefix++;
    }

    return *path == '\0' ? 0 : path;
}

int vfs_read(const char *path, uint64_t offset, uint8_t *buffer,
             uint64_t capacity, uint64_t *bytes_read) {
    const vfs_node_t *node = vfs_find(path);
    uint64_t size;
    int status;

    if (bytes_read != 0) {
        *bytes_read = 0;
    }

    if (node == 0 || node->read == 0 || buffer == 0 || bytes_read == 0 ||
        vfs_node_size(node, &size) != 0 || offset > size) {
        return -1;
    }

    status = node->read(node->context, offset, buffer, capacity, bytes_read);
    if (status != 0 ||
        !vfs_read_result_valid(offset, size, capacity, *bytes_read)) {
        *bytes_read = 0;
        return status != 0 ? status : -1;
    }

    return 0;
}

int vfs_write(const char *path, uint64_t offset, const uint8_t *buffer,
              uint64_t size, uint64_t *bytes_written) {
    const vfs_node_t *node = vfs_find(path);
    uint64_t current_size;
    int status;

    if (bytes_written != 0) {
        *bytes_written = 0;
    }

    if (node == 0 || node->write == 0 || buffer == 0 ||
        bytes_written == 0 || vfs_node_size(node, &current_size) != 0 ||
        offset > current_size) {
        return -1;
    }

    status = node->write(node->context, offset, buffer, size, bytes_written);
    if (status != 0 ||
        !vfs_write_result_valid(offset, size, *bytes_written)) {
        *bytes_written = 0;
        return status != 0 ? status : -1;
    }

    return 0;
}

int vfs_stat(const char *path, vfs_stat_t *stat) {
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

static int vfs_list_emit_byte(uint64_t offset, uint8_t *buffer,
                              uint64_t capacity, uint64_t *position,
                              uint64_t *out, uint8_t value) {
    if (*position >= offset) {
        if (*out >= capacity) {
            return -1;
        }
        buffer[*out] = value;
        (*out)++;
    }
    (*position)++;
    return 0;
}

int vfs_list_at(const char *path, uint64_t offset, uint8_t *buffer,
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

    for (uint32_t i = 0; i < g_vfs_node_count; i++) {
        const char *node_path = g_vfs_nodes[i].path;

        if (node_path == 0) {
            continue;
        }

        while (*node_path != '\0') {
            if (vfs_list_emit_byte(offset, buffer, capacity, &position, &out,
                                   (uint8_t)*node_path) != 0) {
                *bytes_written = out;
                return 0;
            }
            node_path++;
        }

        if (vfs_list_emit_byte(offset, buffer, capacity, &position, &out,
                               (uint8_t)'\n') != 0) {
            *bytes_written = out;
            return 0;
        }
    }

    *bytes_written = out;
    return 0;
}

int vfs_list(const char *path, uint8_t *buffer, uint64_t capacity,
             uint64_t *bytes_written) {
    return vfs_list_at(path, 0, buffer, capacity, bytes_written);
}

int vfs_open_flags(const char *path, uint32_t flags) {
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

int vfs_open(const char *path) {
    return vfs_open_flags(path, VFS_O_RDONLY);
}

int vfs_read_fd(int fd, uint8_t *buffer, uint64_t capacity,
                uint64_t *bytes_read) {
    vfs_open_file_t *file;
    int status;
    uint64_t size;

    if (bytes_read != 0) {
        *bytes_read = 0;
    }

    if (buffer == 0 || bytes_read == 0) {
        return -1;
    }

    file = vfs_fd_at(fd);
    if (file == 0 || file->node->read == 0 ||
        file->flags == VFS_O_WRONLY ||
        vfs_node_size(file->node, &size) != 0 || file->offset > size) {
        return -1;
    }

    status = file->node->read(file->node->context, file->offset, buffer,
                              capacity, bytes_read);
    if (status != 0 ||
        !vfs_read_result_valid(file->offset, size, capacity, *bytes_read)) {
        *bytes_read = 0;
        return status != 0 ? status : -1;
    }

    file->offset += *bytes_read;
    return 0;
}

int vfs_write_fd(int fd, const uint8_t *buffer, uint64_t size,
                 uint64_t *bytes_written) {
    vfs_open_file_t *file;
    int status;
    uint64_t current_size;

    if (bytes_written != 0) {
        *bytes_written = 0;
    }

    if (buffer == 0 || bytes_written == 0) {
        return -1;
    }

    file = vfs_fd_at(fd);
    if (file == 0 || file->node->write == 0 ||
        file->flags == VFS_O_RDONLY ||
        vfs_node_size(file->node, &current_size) != 0 ||
        file->offset > current_size) {
        return -1;
    }

    status = file->node->write(file->node->context, file->offset, buffer,
                               size, bytes_written);
    if (status != 0 ||
        !vfs_write_result_valid(file->offset, size, *bytes_written)) {
        *bytes_written = 0;
        return status != 0 ? status : -1;
    }

    file->offset += *bytes_written;
    return 0;
}

int vfs_seek(int fd, uint64_t offset) {
    uint64_t size;
    vfs_open_file_t *file = vfs_fd_at(fd);

    if (file == 0 || vfs_node_size(file->node, &size) != 0 ||
        offset > size) {
        return -1;
    }

    file->offset = offset;
    return 0;
}

int vfs_close(int fd) {
    vfs_open_file_t *file = vfs_fd_at(fd);

    if (file == 0) {
        return -1;
    }

    vfs_clear_open_file(file);
    return 0;
}

uint32_t vfs_close_all_for_pid(uint32_t pid) {
    uint32_t closed = 0;

    for (uint32_t i = 0; i < VFS_MAX_GLOBAL_OPEN_FILES; i++) {
        if (g_open_files[i].used != 0 &&
            g_open_files[i].owner_pid == pid) {
            vfs_clear_open_file(&g_open_files[i]);
            closed++;
        }
    }

    return closed;
}

int vfs_unlink(const char *path) {
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
