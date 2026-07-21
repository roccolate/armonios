#include <stdint.h>
#include <stdlib.h>

#include "kernel/ipc.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/process.h"
#include "kernel/syscall_argv.h"
#include "kernel/syscall_helpers.h"
#include "kernel/syscall_internal.h"

#define TEST_PMM_PAGES 64U

static void boundary_check(int ok) {
    if (!ok) {
        __builtin_trap();
    }
}

#define CHECK_TRUE(expr) boundary_check((expr) != 0)
#define CHECK_EQ(expected, actual) boundary_check((expected) == (actual))

static int text_equals(const char *actual, const char *expected) {
    uint32_t i = 0U;

    if (actual == 0 || expected == 0) {
        return 0;
    }
    for (;;) {
        if (actual[i] != expected[i]) {
            return 0;
        }
        if (actual[i] == '\0') {
            return 1;
        }
        i++;
    }
}

static void store_u64(uint8_t *dst, uint64_t value) {
    for (uint32_t i = 0; i < sizeof(uint64_t); i++) {
        dst[i] = (uint8_t)(value >> (i * 8U));
    }
}

static void test_syscall_boundary_copies(void) {
    void *pmm_memory = 0;
    uint8_t *user_rw = 0;
    uint8_t *user_ro = 0;
    uint64_t *pgd;
    process_t process;
    panel_boot_argv_t kernel_argv;
    uint64_t rw_base;
    uint64_t ro_base;
    static const char arg0[] = "editor";
    static const char arg1[] = "notes.txt";
    static const uint8_t message[] = {0x10U, 0x20U, 0x30U, 0x40U};

    CHECK_EQ(0, posix_memalign(&pmm_memory, PAGE_SIZE,
                               TEST_PMM_PAGES * PAGE_SIZE));
    pmm_init((uint64_t)(uintptr_t)pmm_memory, TEST_PMM_PAGES * PAGE_SIZE);
    pgd = vmm_new_table();
    CHECK_TRUE(pgd != 0);

    CHECK_EQ(0, posix_memalign((void **)&user_rw, PAGE_SIZE, PAGE_SIZE));
    CHECK_EQ(0, posix_memalign((void **)&user_ro, PAGE_SIZE, PAGE_SIZE));
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        user_rw[i] = 0U;
        user_ro[i] = 0U;
    }

    rw_base = (uint64_t)(uintptr_t)user_rw;
    ro_base = (uint64_t)(uintptr_t)user_ro;
    process_init(&process, 301U, "sys-boundary-copy");
    process_set_page_table(&process, pgd);
    CHECK_EQ(0, process_add_user_region(&process, rw_base, PAGE_SIZE));
    CHECK_EQ(0, process_add_user_region(&process, ro_base, PAGE_SIZE));
    CHECK_EQ(0, vmm_map_page(pgd, rw_base, rw_base,
                             VMM_FLAG_READ | VMM_FLAG_WRITE |
                                 VMM_FLAG_USER));
    CHECK_EQ(0, vmm_map_page(pgd, ro_base, ro_base,
                             VMM_FLAG_READ | VMM_FLAG_USER));

    for (uint32_t i = 0; i < sizeof(arg0); i++) {
        user_rw[512U + i] = (uint8_t)arg0[i];
    }
    for (uint32_t i = 0; i < sizeof(arg1); i++) {
        user_rw[640U + i] = (uint8_t)arg1[i];
    }
    store_u64(&user_rw[128U], rw_base + 512U);
    store_u64(&user_rw[136U], rw_base + 640U);

    CHECK_EQ(0, sys_copy_argv_from_user(&process, rw_base + 128U, 2U,
                                        &kernel_argv));
    CHECK_EQ(0U, kernel_argv.offsets[0]);
    CHECK_EQ(sizeof(arg0), kernel_argv.offsets[1]);
    CHECK_EQ(sizeof(arg0) + sizeof(arg1), kernel_argv.bytes_used);
    CHECK_TRUE(text_equals(&kernel_argv.bytes[kernel_argv.offsets[0]], arg0));
    CHECK_TRUE(text_equals(&kernel_argv.bytes[kernel_argv.offsets[1]], arg1));
    CHECK_EQ(ERR_INVAL,
             sys_copy_argv_from_user(&process, rw_base + 128U,
                                     PANEL_BOOT_ARGV_MAX_STRINGS + 1U,
                                     &kernel_argv));
    CHECK_EQ(ERR_INVAL,
             sys_copy_argv_from_user(&process, 1U, 1U, &kernel_argv));
    CHECK_EQ(0, sys_copy_argv_from_user(&process, 0U, 0U, &kernel_argv));
    CHECK_EQ(ERR_INVAL,
             sys_copy_argv_from_user(&process, rw_base, 0U, &kernel_argv));

    ipc_init();
    for (uint32_t i = 0; i < sizeof(message); i++) {
        user_rw[1024U + i] = message[i];
    }
    CHECK_EQ((int64_t)sizeof(message),
             sys_ipc_send(&process, process.pid, rw_base + 1024U,
                          sizeof(message)));
    for (uint32_t i = 0; i < sizeof(message); i++) {
        user_rw[1024U + i] = 0U;
    }

    CHECK_EQ(ERR_PERM,
             sys_ipc_recv(&process, ro_base, IPC_MAX_MESSAGE_SIZE));
    CHECK_EQ((int64_t)sizeof(message),
             sys_ipc_recv(&process, rw_base + 1152U,
                          IPC_MAX_MESSAGE_SIZE));
    for (uint32_t i = 0; i < sizeof(message); i++) {
        CHECK_EQ(message[i], user_rw[1152U + i]);
    }
    CHECK_EQ(ERR_AGAIN,
             sys_ipc_recv(&process, rw_base + 1152U,
                          IPC_MAX_MESSAGE_SIZE));
    CHECK_EQ(ERR_INVAL,
             sys_ipc_send(&process, process.pid, 1U, sizeof(message)));

    vmm_free_table(pgd);
    free(user_ro);
    free(user_rw);
    free(pmm_memory);
}

__attribute__((constructor))
static void test_syscall_boundary_user_copy_constructor(void) {
    test_syscall_boundary_copies();
}
