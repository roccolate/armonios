#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel/fat32.h"
#include "kernel/process.h"
#include "kernel/vfs.h"

typedef struct {
    const uint8_t *data;
    uint64_t size;
} memory_file_t;

static process_t g_first;
static process_t g_second;
static process_t *g_current;

process_t *process_current(void) {
    return g_current;
}

process_t *process_find(uint32_t pid) {
    if (g_first.pid == pid && g_first.state != PROCESS_UNUSED) {
        return &g_first;
    }
    if (g_second.pid == pid && g_second.state != PROCESS_UNUSED) {
        return &g_second;
    }
    return 0;
}

fat32_fs_t *fat32_default_fs(void) {
    return 0;
}

int fat32_open_root(fat32_fs_t *fs, const char *name, fat32_file_t *file) {
    (void)fs;
    (void)name;
    (void)file;
    return -1;
}

int fat32_create(fat32_fs_t *fs, const char *name, fat32_file_t *file) {
    (void)fs;
    (void)name;
    (void)file;
    return -1;
}

int fat32_mount_vfs_file(fat32_fs_t *fs, const char *path,
                         const char *name) {
    (void)fs;
    (void)path;
    (void)name;
    return -1;
}

int fat32_delete(fat32_fs_t *fs, const char *name) {
    (void)fs;
    (void)name;
    return -1;
}

int fat32_rename(fat32_fs_t *fs, const char *old_name,
                 const char *new_name) {
    (void)fs;
    (void)old_name;
    (void)new_name;
    return -1;
}

static int memory_read(void *context, uint64_t offset, uint8_t *buffer,
                       uint64_t capacity, uint64_t *bytes_read) {
    memory_file_t *file = (memory_file_t *)context;
    uint64_t count = 0;

    if (file == 0 || buffer == 0 || bytes_read == 0 || offset > file->size) {
        return -1;
    }

    while (count < capacity && offset + count < file->size) {
        buffer[count] = file->data[offset + count];
        count++;
    }

    *bytes_read = count;
    return 0;
}

static void reset_processes(void) {
    g_first = (process_t){0};
    g_second = (process_t){0};
    g_first.pid = 10U;
    g_first.state = PROCESS_READY;
    g_second.pid = 20U;
    g_second.state = PROCESS_READY;
    g_current = 0;
}

static void mount_test_files(void) {
    static const uint8_t first_data[] = "alpha";
    static const uint8_t second_data[] = "bravo";
    static memory_file_t first_file = {first_data, 5U};
    static memory_file_t second_file = {second_data, 5U};
    vfs_node_t nodes[2] = {
        {"/a", 5U, memory_read, 0, 0, &first_file},
        {"/b", 5U, memory_read, 0, 0, &second_file},
    };

    vfs_reset();
    assert(vfs_mount_static(nodes, 2U) == 0);
}

static uint8_t read_one(int fd) {
    uint8_t byte = 0;
    uint64_t bytes_read = 0;

    assert(vfs_read_fd(fd, &byte, 1U, &bytes_read) == 0);
    assert(bytes_read == 1U);
    return byte;
}

static void test_same_local_fd_is_process_private(void) {
    int first_fd;
    int second_fd;

    reset_processes();
    mount_test_files();

    g_current = &g_first;
    first_fd = vfs_open("/a");
    assert(first_fd == 0);

    g_current = &g_second;
    second_fd = vfs_open("/b");
    assert(second_fd == 0);
    assert(read_one(second_fd) == (uint8_t)'b');

    g_current = &g_first;
    assert(read_one(first_fd) == (uint8_t)'a');
}

static void test_offsets_and_close_are_isolated(void) {
    int first_fd;
    int second_fd;

    reset_processes();
    mount_test_files();

    g_current = &g_first;
    first_fd = vfs_open("/a");
    assert(first_fd == 0);
    assert(read_one(first_fd) == (uint8_t)'a');

    g_current = &g_second;
    second_fd = vfs_open("/a");
    assert(second_fd == 0);
    assert(read_one(second_fd) == (uint8_t)'a');
    assert(vfs_close(second_fd) == 0);

    g_current = &g_first;
    assert(read_one(first_fd) == (uint8_t)'l');
}

static void test_foreign_unused_fd_is_rejected(void) {
    int first_fd;
    int first_extra_fd;

    reset_processes();
    mount_test_files();

    g_current = &g_first;
    first_fd = vfs_open("/a");
    first_extra_fd = vfs_open("/b");
    assert(first_fd == 0);
    assert(first_extra_fd == 1);

    g_current = &g_second;
    assert(vfs_open("/a") == 0);
    assert(vfs_close(first_extra_fd) == -1);

    g_current = &g_first;
    assert(read_one(first_extra_fd) == (uint8_t)'b');
}

static void test_dead_owner_capacity_is_reclaimed(void) {
    reset_processes();
    mount_test_files();

    g_current = &g_first;
    for (uint32_t i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        assert(vfs_open("/a") == (int)i);
    }
    assert(vfs_open("/a") == -1);

    g_first.state = PROCESS_ZOMBIE;
    g_current = &g_second;
    assert(vfs_open("/b") == 0);
    assert(vfs_close_all_for_pid(g_second.pid) == 1U);
    assert(vfs_close(0) == -1);
}

int main(void) {
    test_same_local_fd_is_process_private();
    test_offsets_and_close_are_isolated();
    test_foreign_unused_fd_is_rejected();
    test_dead_owner_capacity_is_reclaimed();

    puts("PASS: process-local VFS descriptors");
    return 0;
}
