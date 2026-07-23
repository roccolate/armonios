#include <stdint.h>

#include "../kernel/vfs.h"

#if defined(ARMONIOS_VFS_PATH_STANDALONE)
#include "../kernel/process.h"

process_t *process_current(void) {
    return 0;
}

process_t *process_find(uint32_t pid) {
    (void)pid;
    return 0;
}
#endif

typedef struct {
    uint32_t open_calls;
    uint32_t unlink_calls;
    uint32_t rename_calls;
    uint8_t byte;
    const char *expected_open;
    char last_path[VFS_MAX_PATH];
    char last_new_path[VFS_MAX_PATH];
    vfs_node_t node;
} test_mount_t;

static void check_true(int ok) {
    if (!ok) {
        __builtin_trap();
    }
}

#define CHECK_TRUE(expr) check_true((expr) != 0)
#define CHECK_EQ(expected, actual) check_true((expected) == (actual))

static int text_equal(const char *left, const char *right) {
    uint32_t i = 0;

    if (left == 0 || right == 0) {
        return left == right;
    }
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) {
            return 0;
        }
        i++;
    }
    return left[i] == right[i];
}

static int copy_text(char destination[VFS_MAX_PATH], const char *source) {
    uint32_t i = 0;

    if (destination == 0 || source == 0) {
        return -1;
    }
    while (source[i] != '\0') {
        if (i + 1U >= VFS_MAX_PATH) {
            return -1;
        }
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
    return 0;
}

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

    if (mount == 0 || path == 0 || flags != VFS_O_RDONLY ||
        (mount->expected_open != 0 &&
         !text_equal(path, mount->expected_open)) ||
        copy_text(mount->last_path, path) != 0) {
        return -1;
    }
    mount->open_calls++;

    mount->node.path = path;
    mount->node.size = 1;
    mount->node.read = mounted_read;
    mount->node.write = 0;
    mount->node.stat = 0;
    mount->node.context = mount;
    return vfs_mount_static(&mount->node, 1);
}

static int mount_unlink(void *context, const char *path) {
    test_mount_t *mount = (test_mount_t *)context;

    if (mount == 0 || path == 0 ||
        copy_text(mount->last_path, path) != 0) {
        return -1;
    }
    mount->unlink_calls++;
    return 0;
}

static int mount_rename(void *context, const char *old_path,
                        const char *new_path) {
    test_mount_t *mount = (test_mount_t *)context;

    if (mount == 0 || old_path == 0 || new_path == 0 ||
        copy_text(mount->last_path, old_path) != 0 ||
        copy_text(mount->last_new_path, new_path) != 0) {
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

static void test_path_normalization_contract(void) {
    char normalized[VFS_MAX_PATH];
    char too_long[VFS_MAX_PATH + 1U];

    CHECK_EQ(0, vfs_normalize_path("/fat//a/./b/../c.txt/", normalized));
    CHECK_TRUE(text_equal(normalized, "/fat/a/c.txt"));

    CHECK_EQ(0, vfs_normalize_path("////./", normalized));
    CHECK_TRUE(text_equal(normalized, "/"));

    CHECK_EQ(0, vfs_normalize_path("/a/b/../../", normalized));
    CHECK_TRUE(text_equal(normalized, "/"));

    CHECK_EQ(-1, vfs_normalize_path(0, normalized));
    CHECK_EQ(-1, vfs_normalize_path("", normalized));
    CHECK_EQ(-1, vfs_normalize_path("relative/path", normalized));
    CHECK_EQ(-1, vfs_normalize_path("/../escape", normalized));
    CHECK_EQ(-1, vfs_normalize_path("/a/../../escape", normalized));

    too_long[0] = '/';
    for (uint32_t i = 1; i < VFS_MAX_PATH; i++) {
        too_long[i] = 'x';
    }
    too_long[VFS_MAX_PATH] = '\0';
    CHECK_EQ(-1, vfs_normalize_path(too_long, normalized));
}

static void test_open_routes_through_owning_mount(void) {
    test_mount_t mount = {
        .byte = 0x5a,
        .expected_open = "/disk/item",
    };
    uint8_t byte = 0;
    uint64_t count = 0;
    int fd;

    vfs_reset();
    CHECK_EQ(0, vfs_mount("/disk", &test_mount_ops, &mount));

    fd = vfs_open("/disk/item");
    CHECK_TRUE(fd >= 0);
    CHECK_EQ(1U, mount.open_calls);
    CHECK_TRUE(text_equal(mount.last_path, "/disk/item"));
    CHECK_EQ(0, vfs_read_fd(fd, &byte, 1, &count));
    CHECK_EQ(1U, count);
    CHECK_EQ(0x5aU, byte);
    CHECK_EQ(0, vfs_close(fd));

    CHECK_EQ(-1, vfs_open("/diskette/item"));
    CHECK_EQ(1U, mount.open_calls);
}

static void test_canonical_mount_resolution_prefers_longest_prefix(void) {
    test_mount_t disk = {
        .byte = 0x11,
    };
    test_mount_t nested = {
        .byte = 0x22,
        .expected_open = "/disk/sub/item",
    };
    test_mount_t duplicate = { 0 };
    uint8_t byte = 0;
    uint64_t count = 0;
    int fd;

    vfs_reset();
    CHECK_EQ(0, vfs_mount("/disk/", &test_mount_ops, &disk));
    CHECK_EQ(0, vfs_mount("//disk/sub/./", &test_mount_ops, &nested));
    CHECK_EQ(-1, vfs_mount("/disk//sub", &test_mount_ops, &duplicate));

    fd = vfs_open("/disk//sub/./tmp/../item");
    CHECK_TRUE(fd >= 0);
    CHECK_EQ(0U, disk.open_calls);
    CHECK_EQ(1U, nested.open_calls);
    CHECK_TRUE(text_equal(nested.last_path, "/disk/sub/item"));
    CHECK_EQ(0, vfs_read_fd(fd, &byte, 1, &count));
    CHECK_EQ(1U, count);
    CHECK_EQ(0x22U, byte);
    CHECK_EQ(0, vfs_close(fd));

    CHECK_EQ(-1, vfs_open("/disk/sub/../../outside"));
    CHECK_EQ(0U, disk.open_calls);
    CHECK_EQ(1U, nested.open_calls);
    CHECK_EQ(-1, vfs_open("/disk/sub/../../../escape"));
}

static void test_static_nodes_use_canonical_identity(void) {
    vfs_node_t node = {
        .path = "/boot//tools/./../app/",
        .size = 1,
        .read = mounted_read,
    };
    vfs_node_t duplicate = {
        .path = "/boot/x/../app",
        .size = 1,
        .read = mounted_read,
    };

    vfs_reset();
    CHECK_EQ(0, vfs_mount_static(&node, 1));
    CHECK_TRUE(vfs_find("/boot/./app") != 0);
    CHECK_EQ(-1, vfs_mount_static(&duplicate, 1));
    CHECK_EQ(0, vfs_unmount_static("//boot/app/"));
    CHECK_TRUE(vfs_find("/boot/app") == 0);
}

static void test_mutation_stays_inside_one_canonical_mount(void) {
    test_mount_t disk = { 0 };
    test_mount_t nested = { 0 };
    test_mount_t other = { 0 };

    vfs_reset();
    CHECK_EQ(0, vfs_mount("/disk", &test_mount_ops, &disk));
    CHECK_EQ(0, vfs_mount("/disk/sub", &test_mount_ops, &nested));
    CHECK_EQ(0, vfs_mount("/other", &test_mount_ops, &other));

    CHECK_EQ(0, vfs_unlink("/disk/sub/tmp/../item"));
    CHECK_EQ(0U, disk.unlink_calls);
    CHECK_EQ(1U, nested.unlink_calls);
    CHECK_TRUE(text_equal(nested.last_path, "/disk/sub/item"));

    CHECK_EQ(0, vfs_rename("/disk/sub/old/../item",
                           "/disk/sub/new/./name"));
    CHECK_EQ(1U, nested.rename_calls);
    CHECK_TRUE(text_equal(nested.last_path, "/disk/sub/item"));
    CHECK_TRUE(text_equal(nested.last_new_path, "/disk/sub/new/name"));

    CHECK_EQ(-1, vfs_rename("/disk/sub/item",
                            "/disk/sub/../../other/new"));
    CHECK_EQ(1U, nested.rename_calls);
    CHECK_EQ(0U, other.rename_calls);
}

__attribute__((constructor))
static void test_vfs_mount_dispatch_constructor(void) {
    test_path_normalization_contract();
    test_open_routes_through_owning_mount();
    test_canonical_mount_resolution_prefers_longest_prefix();
    test_static_nodes_use_canonical_identity();
    test_mutation_stays_inside_one_canonical_mount();
}

#if defined(ARMONIOS_VFS_PATH_STANDALONE)
int main(void) {
    return 0;
}
#endif
