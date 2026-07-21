#include <stdint.h>
#include <stdlib.h>

#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/process.h"
#include "kernel/syscall_internal.h"
#include "kernel/vfs.h"

#define TEST_PMM_PAGES 64U
#define TEST_IO_SIZE   600U

typedef struct {
    uint8_t data[TEST_IO_SIZE];
    uint32_t read_calls;
    uint32_t write_calls;
    uintptr_t user_start;
    uintptr_t user_end;
} syscall_vfs_test_file_t;

static void check_true(int ok) {
    if (!ok) {
        __builtin_trap();
    }
}

#define CHECK_TRUE(expr) check_true((expr) != 0)
#define CHECK_EQ(expected, actual) check_true((expected) == (actual))

static int buffer_overlaps_user(const syscall_vfs_test_file_t *file,
                                const void *buffer, uint64_t size) {
    uintptr_t start = (uintptr_t)buffer;
    uintptr_t end;

    if (size == 0) {
        return start >= file->user_start && start < file->user_end;
    }
    if (start > UINTPTR_MAX - (uintptr_t)size) {
        return 1;
    }
    end = start + (uintptr_t)size;
    return start < file->user_end && end > file->user_start;
}

static int test_file_read(void *context, uint64_t offset, uint8_t *buffer,
                          uint64_t capacity, uint64_t *bytes_read) {
    syscall_vfs_test_file_t *file = (syscall_vfs_test_file_t *)context;
    uint64_t count;

    if (file == 0 || buffer == 0 || bytes_read == 0 ||
        offset > sizeof(file->data)) {
        return -1;
    }
    CHECK_TRUE(!buffer_overlaps_user(file, buffer, capacity));

    count = sizeof(file->data) - offset;
    if (count > capacity) {
        count = capacity;
    }
    for (uint64_t i = 0; i < count; i++) {
        buffer[i] = file->data[offset + i];
    }
    *bytes_read = count;
    file->read_calls++;
    return 0;
}

static int test_file_write(void *context, uint64_t offset,
                           const uint8_t *buffer, uint64_t size,
                           uint64_t *bytes_written) {
    syscall_vfs_test_file_t *file = (syscall_vfs_test_file_t *)context;

    if (file == 0 || buffer == 0 || bytes_written == 0 ||
        offset > sizeof(file->data) || size > sizeof(file->data) - offset) {
        return -1;
    }
    CHECK_TRUE(!buffer_overlaps_user(file, buffer, size));

    for (uint64_t i = 0; i < size; i++) {
        file->data[offset + i] = buffer[i];
    }
    *bytes_written = size;
    file->write_calls++;
    return 0;
}

static void test_syscall_vfs_uses_kernel_owned_chunks(void) {
    void *pmm_memory = 0;
    uint8_t *user_page = 0;
    uint64_t *pgd;
    process_t process;
    syscall_vfs_test_file_t file = {0};
    vfs_node_t node = {
        .path = "/tmp/syscall-copy",
        .size = TEST_IO_SIZE,
        .read = test_file_read,
        .write = test_file_write,
        .stat = 0,
        .context = &file,
    };
    uint64_t base;
    int fd;

    CHECK_EQ(0, posix_memalign(&pmm_memory, PAGE_SIZE,
                               TEST_PMM_PAGES * PAGE_SIZE));
    pmm_init((uint64_t)(uintptr_t)pmm_memory, TEST_PMM_PAGES * PAGE_SIZE);
    pgd = vmm_new_table();
    CHECK_TRUE(pgd != 0);

    CHECK_EQ(0, posix_memalign((void **)&user_page, PAGE_SIZE, PAGE_SIZE));
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        user_page[i] = 0;
    }

    base = (uint64_t)(uintptr_t)user_page;
    process_init(&process, 201U, "sys-vfs-copy");
    process_set_page_table(&process, pgd);
    CHECK_EQ(0, process_add_user_region(&process, base, PAGE_SIZE));
    CHECK_EQ(0, vmm_map_page(pgd, base, base,
                             VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_USER));

    file.user_start = (uintptr_t)user_page;
    file.user_end = file.user_start + PAGE_SIZE;
    for (uint64_t i = 0; i < TEST_IO_SIZE; i++) {
        user_page[i] = (uint8_t)(i & 0xffU);
    }

    vfs_reset();
    process_set_current(&process);
    CHECK_EQ(0, vfs_mount_static(&node, 1));
    fd = vfs_open_flags("/tmp/syscall-copy", VFS_O_RDWR);
    CHECK_TRUE(fd >= 0);

    CHECK_EQ(TEST_IO_SIZE,
             sys_write(&process, (uint64_t)fd + FD_FILE_BASE, base,
                       TEST_IO_SIZE));
    CHECK_EQ(3U, file.write_calls);
    for (uint64_t i = 0; i < TEST_IO_SIZE; i++) {
        CHECK_EQ((uint8_t)(i & 0xffU), file.data[i]);
        user_page[1024U + i] = 0;
    }

    CHECK_EQ(0, vfs_seek(fd, 0));
    CHECK_EQ(TEST_IO_SIZE,
             sys_read(&process, (uint64_t)fd + FD_FILE_BASE, base + 1024U,
                      TEST_IO_SIZE));
    CHECK_EQ(3U, file.read_calls);
    for (uint64_t i = 0; i < TEST_IO_SIZE; i++) {
        CHECK_EQ(file.data[i], user_page[1024U + i]);
    }

    {
        static const char path[] = "/tmp/syscall-copy";
        vfs_stat_t *user_stat = (vfs_stat_t *)&user_page[3072U];

        for (uint64_t i = 0; i < sizeof(path); i++) {
            user_page[3000U + i] = (uint8_t)path[i];
        }
        user_stat->size = 0;
        CHECK_EQ(0, sys_stat(&process, base + 3000U, base + 3072U));
        CHECK_EQ(TEST_IO_SIZE, user_stat->size);
    }

    CHECK_EQ(0, vfs_close(fd));
    process_set_current(0);
    vfs_reset();
    vmm_free_table(pgd);
    free(user_page);
    free(pmm_memory);
}

__attribute__((constructor))
static void test_syscall_vfs_user_copy_constructor(void) {
    test_syscall_vfs_uses_kernel_owned_chunks();
}
