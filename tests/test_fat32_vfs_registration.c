#include <stdint.h>

#include "../kernel/fat32.h"
#include "../kernel/vfs.h"

static void check_true(int ok) {
    if (!ok) {
        __builtin_trap();
    }
}

#define CHECK_EQ(expected, actual) check_true((expected) == (actual))

static int reject_open(void *context, const char *path, uint32_t flags) {
    (void)context;
    (void)path;
    (void)flags;
    return -1;
}

static const vfs_mount_ops_t reject_ops = {
    .open = reject_open,
};

/*
 * tests/main.c still carries the historical runner symbol. Keep the bridge in
 * test code only; the FAT32 API and production objects no longer expose or use
 * a default filesystem authority.
 */
void test_fat32_mount_state_is_instance_local(void);
void test_fat32_default_fs_tracks_successful_mount_only(void) {
    test_fat32_mount_state_is_instance_local();
}

static void test_fat_root_binding_requires_registered_mount(void) {
    fat32_fs_t fs = {
        .mounted = 1,
    };

    vfs_reset();
    CHECK_EQ(0, vfs_mount("/one", &reject_ops, 0));
    CHECK_EQ(0, vfs_mount("/two", &reject_ops, 0));
    CHECK_EQ(0, vfs_mount("/three", &reject_ops, 0));
    CHECK_EQ(0, vfs_mount("/four", &reject_ops, 0));

    fat32_vfs_reset();
    CHECK_EQ(-1, fat32_mount_vfs_root(&fs, "/fat"));
}

static void test_fat_root_binding_succeeds_after_reset(void) {
    fat32_fs_t fs = {
        .mounted = 1,
    };

    vfs_reset();
    fat32_vfs_reset();
    CHECK_EQ(0, fat32_mount_vfs_root(&fs, "/fat"));
}

__attribute__((constructor))
static void test_fat32_vfs_registration_constructor(void) {
    test_fat_root_binding_requires_registered_mount();
    test_fat_root_binding_succeeds_after_reset();
}
