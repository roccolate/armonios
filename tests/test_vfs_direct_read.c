#include <stdint.h>

#include "../kernel/process.h"
#include "../kernel/vfs.h"

process_t *process_current(void) {
    return 0;
}

process_t *process_find(uint32_t pid) {
    (void)pid;
    return 0;
}

typedef struct {
    uint32_t open_calls;
    uint32_t read_calls;
    uint32_t metadata_calls;
    uint8_t byte;
    uint8_t overreport;
    uint8_t directory;
    char last_path[VFS_MAX_PATH];
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

static int copy_path(char destination[VFS_MAX_PATH], const char *source) {
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

static int test_open(void *context, const char *path, uint32_t flags) {
    test_mount_t *mount = (test_mount_t *)context;

    (void)path;
    (void)flags;
    if (mount == 0) {
        return -1;
    }
    mount->open_calls++;
    return -1;
}

static int test_read_path(void *context, const char *path, uint64_t offset,
                          uint8_t *buffer, uint64_t capacity,
                          uint64_t *bytes_read) {
    test_mount_t *mount = (test_mount_t *)context;

    if (bytes_read != 0) {
        *bytes_read = 0;
    }
    if (mount == 0 || path == 0 || buffer == 0 || bytes_read == 0 ||
        copy_path(mount->last_path, path) != 0) {
        return -1;
    }

    mount->read_calls++;
    if (mount->overreport != 0U) {
        *bytes_read = capacity + 1U;
        return 0;
    }
    if (offset >= 1U || capacity == 0U) {
        return 0;
    }

    buffer[0] = mount->byte;
    *bytes_read = 1U;
    return 0;
}

static int test_metadata(void *context, const char *path,
                         vfs_metadata_t *metadata) {
    test_mount_t *mount = (test_mount_t *)context;

    if (mount == 0 || path == 0 || metadata == 0) {
        return -1;
    }
    mount->metadata_calls++;
    metadata->size = mount->directory != 0U ? 0U : 1U;
    metadata->type = mount->directory != 0U
                         ? VFS_FILE_TYPE_DIRECTORY
                         : VFS_FILE_TYPE_REGULAR;
    metadata->attributes = 0U;
    return 0;
}

static int static_read(void *context, uint64_t offset, uint8_t *buffer,
                       uint64_t capacity, uint64_t *bytes_read) {
    const uint8_t *value = (const uint8_t *)context;

    if (bytes_read != 0) {
        *bytes_read = 0;
    }
    if (value == 0 || buffer == 0 || bytes_read == 0) {
        return -1;
    }
    if (offset >= 1U || capacity == 0U) {
        return 0;
    }
    buffer[0] = *value;
    *bytes_read = 1U;
    return 0;
}

static const vfs_mount_ops_t test_ops = {
    .open = test_open,
    .read_path = test_read_path,
    .metadata_path = test_metadata,
};

static void test_direct_read_uses_canonical_path_without_fd(void) {
    test_mount_t mount = {
        .byte = 0x5aU,
    };
    uint8_t byte = 0U;
    uint64_t count = 0U;

    vfs_reset();
    CHECK_EQ(0, vfs_mount("/disk", &test_ops, &mount));
    CHECK_EQ(0, vfs_read("/disk//tmp/../HELLO.KLI", 0U, &byte, 1U,
                         &count));
    CHECK_EQ(0x5aU, byte);
    CHECK_EQ(1U, count);
    CHECK_EQ(1U, mount.read_calls);
    CHECK_EQ(1U, mount.metadata_calls);
    CHECK_EQ(0U, mount.open_calls);
    CHECK_TRUE(text_equal(mount.last_path, "/disk/HELLO.KLI"));
}

static void test_direct_read_rejects_directory_and_overreport(void) {
    test_mount_t mount = {
        .byte = 0x33U,
        .directory = 1U,
    };
    uint8_t byte = 0U;
    uint64_t count = 9U;

    vfs_reset();
    CHECK_EQ(0, vfs_mount("/disk", &test_ops, &mount));
    CHECK_EQ(-1, vfs_read("/disk/DIR", 0U, &byte, 1U, &count));
    CHECK_EQ(0U, count);
    CHECK_EQ(0U, mount.read_calls);

    mount.directory = 0U;
    mount.overreport = 1U;
    count = 9U;
    CHECK_EQ(-1, vfs_read("/disk/BAD.KLI", 0U, &byte, 1U, &count));
    CHECK_EQ(0U, count);
    CHECK_EQ(1U, mount.read_calls);
    CHECK_EQ(0U, mount.open_calls);
}

static void test_static_node_keeps_precedence(void) {
    test_mount_t mount = {
        .byte = 0x11U,
    };
    uint8_t static_byte = 0xa5U;
    vfs_node_t node = {
        .path = "/disk/HELLO.KLI",
        .size = 1U,
        .read = static_read,
        .context = &static_byte,
    };
    uint8_t byte = 0U;
    uint64_t count = 0U;

    vfs_reset();
    CHECK_EQ(0, vfs_mount("/disk", &test_ops, &mount));
    CHECK_EQ(0, vfs_mount_static(&node, 1U));
    CHECK_EQ(0, vfs_read("/disk/HELLO.KLI", 0U, &byte, 1U, &count));
    CHECK_EQ(0xa5U, byte);
    CHECK_EQ(1U, count);
    CHECK_EQ(0U, mount.read_calls);
    CHECK_EQ(0U, mount.metadata_calls);
    CHECK_EQ(0U, mount.open_calls);
}

int main(void) {
    test_direct_read_uses_canonical_path_without_fd();
    test_direct_read_rejects_directory_and_overreport();
    test_static_node_keeps_precedence();
    return 0;
}
