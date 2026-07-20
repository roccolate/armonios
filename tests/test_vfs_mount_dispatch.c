#include <stdint.h>

#include "../kernel/vfs.h"

typedef struct {
    uint32_t open_calls;
    uint32_t unlink_calls;
    uint32_t rename_calls;
    uint8_t byte;
    vfs_node_t node;
} test_mount_t;

static void check_true(int ok) {
    if (!ok) {
        __builtin_trap();
    }
}

#define CHECK_TRUE(expr) check_true((expr) != 0)
#define CHECK_EQ(expected, actual) check_true((expected) == (actual))

static int mounted_read(void *context, uint64_t offset, uint8_t *buffer,
                        uint64_t capacity, uint64_t *bytes_read) {
    test_mount_t *mount = (test_mount_t *)context;

    if (bytes_read != 0) {
        *bytes_read = 0;
    }
    if (mount == 0 || buffer == 0 || bytes_read == 0 || offset > 1U) {
        return -1;
    }
    if (offset == 1U || capacity == 0) {
        return 0;
    }

    buffer[0] = mount->byte;
    *bytes_read = 1;
    return 0;
}

static int mount_open(void *context, const char *path, uint32_t flags) {
    test_mount_t *mount = (test_mount_t *)context;

    if (mount == 0 || path == 0 || flags != VFS_O_RDONLY) {
        return -1;
    }
    mount->open_calls++;
    if (path[0] != '/' || path[1] != 'd' || path[2] != 'i' ||
        path[3] != 's' || path[4] != 'k' || path[5] != '/' ||
        path[6] != 'i' || path[7] != 't' || path[8] != 'e' ||
        path[9] != 'm' || path[10] != '\0') {
        return -1;
    }

    mount->node.path = "/disk/item";
    mount->node.size = 1;
    mount->node.read = mounted_read;
    mount->node.write = 0;
    mount->node.stat = 0;
    mount->node.context = mount;
    return vfs_mount_static(&mount->node, 1);
}

static int mount_unlink(void *context, const char *path) {
    test_mount_t *mount = (test_mount_t *)context;

    if (mount == 0 || path == 0) {
        return -1;
    }
    mount->unlink_calls++;
    return 0;
}

static int mount_rename(void *context, const char *old_path,
                        const char *new_path) {
    test_mount_t *mount = (test_mount_t *)context;

    if (mount == 0 || old_path == 0 || new_path == 0) {
        return -1;
    }
    mount->rename_calls++;
    return 0;
}

static const vfs_mount_ops_t test_mount_ops = {
    .open = mount_open,
    .unlink = mount_unlink,
    .rename = mount_rename,
};

static void test_open_routes_through_owning_mount(void) {
    test_mount_t mount = {
        .byte = 0x5a,
    };
    uint8_t byte = 0;
    uint64_t count = 0;
    int fd;

    vfs_reset();
    CHECK_EQ(0, vfs_mount("/disk", &test_mount_ops, &mount));

    fd = vfs_open("/disk/item");
    CHECK_TRUE(fd >= 0);
    CHECK_EQ(1U, mount.open_calls);
    CHECK_EQ(0, vfs_read_fd(fd, &byte, 1, &count));
    CHECK_EQ(1U, count);
    CHECK_EQ(0x5aU, byte);
    CHECK_EQ(0, vfs_close(fd));

    CHECK_EQ(-1, vfs_open("/diskette/item"));
    CHECK_EQ(1U, mount.open_calls);
}

static void test_mutation_stays_inside_one_mount(void) {
    test_mount_t disk = { 0 };
    test_mount_t other = { 0 };

    vfs_reset();
    CHECK_EQ(0, vfs_mount("/disk", &test_mount_ops, &disk));
    CHECK_EQ(0, vfs_mount("/other", &test_mount_ops, &other));

    CHECK_EQ(0, vfs_unlink("/disk/item"));
    CHECK_EQ(1U, disk.unlink_calls);
    CHECK_EQ(0U, other.unlink_calls);

    CHECK_EQ(0, vfs_rename("/disk/old", "/disk/new"));
    CHECK_EQ(1U, disk.rename_calls);

    CHECK_EQ(-1, vfs_rename("/disk/old", "/other/new"));
    CHECK_EQ(1U, disk.rename_calls);
    CHECK_EQ(0U, other.rename_calls);
}

__attribute__((constructor))
static void test_vfs_mount_dispatch_constructor(void) {
    test_open_routes_through_owning_mount();
    test_mutation_stays_inside_one_mount();
}
