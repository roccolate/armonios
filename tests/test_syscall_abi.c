/*
 * test_syscall_abi.c
 *
 * Lock down the public ArmoniOS syscall ABI. Public headers under
 * include/armonios/abi/ are the source of truth shared by kernel and userland;
 * compatibility names in kernel and libkarm must remain exact aliases.
 *
 * The test only exercises lightweight public headers on purpose: the full
 * syscall_dispatch path pulls in mmu, sched, timer and a working UART, none of
 * which the host suite provides. The end-to-end "does the kernel honour the
 * ABI" question is answered by the tests that drive the same code paths via
 * the public kernel/process, kernel/vfs and kernel/gui APIs.
 */

#include "unity/unity.h"

#include <stdint.h>

#include "include/armonios/abi/base.h"
#include "include/armonios/abi/errors.h"
#include "include/armonios/abi/memory.h"
#include "include/armonios/abi/process.h"
#include "include/armonios/abi/syscall_numbers.h"
#include "include/armonios/abi/system.h"
#include "include/armonios/abi/version.h"
#include "include/armonios/abi/vfs.h"
#include "kernel/process.h"
#include "kernel/syscall_helpers.h"
#include "kernel/syscall_numbers.h"
#include "kernel/user_exit.h"
#include "kernel/user_vm.h"
#include "kernel/vfs.h"

void test_syscall_abi_implemented_numbers_match_dispatch(void) {
    /* The public ABI revision is explicit even before runtime capability
     * discovery exists. Incrementing it must be an intentional contract edit. */
    TEST_ASSERT_EQUAL_UINT64(1U, ARMONIOS_ABI_MAJOR);
    TEST_ASSERT_EQUAL_UINT64(0U, ARMONIOS_ABI_MINOR);
    TEST_ASSERT_EQUAL_UINT64(0x00010000U, ARMONIOS_ABI_VERSION);

    TEST_ASSERT_EQUAL_UINT64(8U, sizeof(arm_status_t));
    TEST_ASSERT_EQUAL_UINT64(4U, sizeof(arm_pid_t));
    TEST_ASSERT_EQUAL_UINT64(4U, sizeof(arm_fd_t));
    TEST_ASSERT_EQUAL_UINT64(16U, sizeof(arm_meminfo_t));
    TEST_ASSERT_EQUAL_UINT64(24U, sizeof(arm_timeinfo_t));
    TEST_ASSERT_EQUAL_UINT64(24U, sizeof(arm_process_entry_t));
    TEST_ASSERT_EQUAL_UINT64(8U, sizeof(arm_stat_t));
    TEST_ASSERT_EQUAL_UINT64(32U, sizeof(arm_stat_v2_t));
    TEST_ASSERT_EQUAL_UINT64(96U, sizeof(arm_dirent_v2_t));

    /* Implemented numbers must match the rows in docs/SYSCALLS.md under
     * "Implemented Now". A drift here means a number was renumbered
     * without updating the docs and breaks every userland image. */
    TEST_ASSERT_EQUAL_UINT64(1ULL, SYS_EXIT);
    TEST_ASSERT_EQUAL_UINT64(2ULL, SYS_YIELD);
    TEST_ASSERT_EQUAL_UINT64(3ULL, SYS_GETPID);
    TEST_ASSERT_EQUAL_UINT64(4ULL, SYS_SPAWN);
    TEST_ASSERT_EQUAL_UINT64(6ULL, SYS_WAIT);
    TEST_ASSERT_EQUAL_UINT64(7ULL, SYS_KILL);
    TEST_ASSERT_EQUAL_UINT64(8ULL, SYS_SPAWN_ARGV);
    TEST_ASSERT_EQUAL_UINT64(20ULL, SYS_MMAP);
    TEST_ASSERT_EQUAL_UINT64(21ULL, SYS_MUNMAP);
    TEST_ASSERT_EQUAL_UINT64(40ULL, SYS_OPEN);
    TEST_ASSERT_EQUAL_UINT64(41ULL, SYS_CLOSE);
    TEST_ASSERT_EQUAL_UINT64(42ULL, SYS_READ);
    TEST_ASSERT_EQUAL_UINT64(43ULL, SYS_WRITE);
    TEST_ASSERT_EQUAL_UINT64(44ULL, SYS_SEEK);
    TEST_ASSERT_EQUAL_UINT64(45ULL, SYS_STAT);
    TEST_ASSERT_EQUAL_UINT64(46ULL, SYS_READDIR);
    TEST_ASSERT_EQUAL_UINT64(47ULL, SYS_UNLINK);
    TEST_ASSERT_EQUAL_UINT64(48ULL, SYS_RENAME);
    TEST_ASSERT_EQUAL_UINT64(49ULL, SYS_STAT_V2);
    TEST_ASSERT_EQUAL_UINT64(50ULL, SYS_READDIR_V2);
    TEST_ASSERT_EQUAL_UINT64(60ULL, SYS_IPC_SEND);
    TEST_ASSERT_EQUAL_UINT64(61ULL, SYS_IPC_RECV);
    TEST_ASSERT_EQUAL_UINT64(70ULL, SYS_WINDOW_CREATE);
    TEST_ASSERT_EQUAL_UINT64(71ULL, SYS_WINDOW_DESTROY);
    TEST_ASSERT_EQUAL_UINT64(72ULL, SYS_WINDOW_DRAW_TEXT);
    TEST_ASSERT_EQUAL_UINT64(73ULL, SYS_WINDOW_DRAW_RECT);
    TEST_ASSERT_EQUAL_UINT64(74ULL, SYS_WINDOW_EVENT);
    TEST_ASSERT_EQUAL_UINT64(75ULL, SYS_WINDOW_SET_TITLE);
    TEST_ASSERT_EQUAL_UINT64(76ULL, SYS_WINDOW_REDRAW);
    TEST_ASSERT_EQUAL_UINT64(77ULL, SYS_WINDOW_FOCUS);
    TEST_ASSERT_EQUAL_UINT64(78ULL, SYS_WINDOW_FOR_PID);
    TEST_ASSERT_EQUAL_UINT64(79ULL, SYS_CURSOR_SET_SHAPE);
    TEST_ASSERT_EQUAL_UINT64(80ULL, SYS_WINDOW_FLUSH);
    TEST_ASSERT_EQUAL_UINT64(81ULL, SYS_WINDOW_GET_BOUNDS);
    TEST_ASSERT_EQUAL_UINT64(82ULL, SYS_WINDOW_SET_BOUNDS);
    TEST_ASSERT_EQUAL_UINT64(83ULL, SYS_WINDOW_MINIMIZE);
    TEST_ASSERT_EQUAL_UINT64(84ULL, SYS_WINDOW_RESTORE);
    TEST_ASSERT_EQUAL_UINT64(85ULL, SYS_WINDOW_STATE);
    TEST_ASSERT_EQUAL_UINT64(86ULL, SYS_CURSOR_REGISTER_REGION);
    TEST_ASSERT_EQUAL_UINT64(100ULL, SYS_TIMEINFO);
    TEST_ASSERT_EQUAL_UINT64(101ULL, SYS_MEMINFO);
    TEST_ASSERT_EQUAL_UINT64(102ULL, SYS_PROCLIST);
}

void test_syscall_abi_ranges_do_not_overlap(void) {
    /* Process, memory, VFS, IPC, window and info ranges must not bleed
     * into each other. An overlap means a syscall lost its number to
     * a neighbour range, which is an ABI break. */
    TEST_ASSERT_TRUE(SYS_KILL < SYS_MMAP);
    TEST_ASSERT_TRUE(SYS_SPAWN_ARGV < SYS_MMAP);
    TEST_ASSERT_TRUE(SYS_MUNMAP < SYS_OPEN);
    TEST_ASSERT_TRUE(SYS_READDIR_V2 < SYS_IPC_SEND);
    TEST_ASSERT_TRUE(SYS_IPC_RECV < SYS_WINDOW_CREATE);
    TEST_ASSERT_TRUE(SYS_WINDOW_FLUSH < SYS_TIMEINFO);
}

void test_syscall_abi_error_codes_match_documented_constants(void) {
    /* Public errors are the source of truth. Kernel and userland compatibility
     * names must remain aliases so old source continues to compile unchanged. */
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)-2,
                             (uint64_t)(int64_t)ARMONIOS_ERR_NOMEM);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)-3,
                             (uint64_t)(int64_t)ARMONIOS_ERR_NOENT);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)-5,
                             (uint64_t)(int64_t)ARMONIOS_ERR_BADF);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)-7,
                             (uint64_t)(int64_t)ARMONIOS_ERR_INVAL);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)-11,
                             (uint64_t)(int64_t)ARMONIOS_ERR_AGAIN);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)-13,
                             (uint64_t)(int64_t)ARMONIOS_ERR_PERM);

    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)ARMONIOS_ERR_NOMEM,
                             (uint64_t)(int64_t)USER_VM_ERR_NOMEM);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)ARMONIOS_ERR_NOENT,
                             (uint64_t)(int64_t)ERR_NOENT);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)ARMONIOS_ERR_BADF,
                             (uint64_t)(int64_t)ERR_BADF);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)ARMONIOS_ERR_INVAL,
                             (uint64_t)(int64_t)ERR_INVAL);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)ARMONIOS_ERR_AGAIN,
                             (uint64_t)(int64_t)ERR_AGAIN);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)ARMONIOS_ERR_PERM,
                             (uint64_t)(int64_t)ERR_PERM);

    TEST_ASSERT_TRUE(ARMONIOS_ERR_NOMEM != ARMONIOS_ERR_NOENT);
    TEST_ASSERT_TRUE(ARMONIOS_ERR_NOENT != ARMONIOS_ERR_BADF);
    TEST_ASSERT_TRUE(ARMONIOS_ERR_BADF != ARMONIOS_ERR_INVAL);
    TEST_ASSERT_TRUE(ARMONIOS_ERR_INVAL != ARMONIOS_ERR_AGAIN);
    TEST_ASSERT_TRUE(ARMONIOS_ERR_AGAIN != ARMONIOS_ERR_PERM);
}

void test_syscall_abi_vfs_open_flags_match_documentation(void) {
    TEST_ASSERT_EQUAL_UINT64(0ULL, ARM_O_RDONLY);
    TEST_ASSERT_EQUAL_UINT64(1ULL, ARM_O_WRONLY);
    TEST_ASSERT_EQUAL_UINT64(2ULL, ARM_O_RDWR);
    TEST_ASSERT_EQUAL_UINT64(0x40ULL, ARM_O_CREAT);
    TEST_ASSERT_EQUAL_UINT64(0x43ULL, ARM_O_ALLOWED);

    TEST_ASSERT_EQUAL_UINT64(ARM_O_RDONLY, VFS_O_RDONLY);
    TEST_ASSERT_EQUAL_UINT64(ARM_O_WRONLY, VFS_O_WRONLY);
    TEST_ASSERT_EQUAL_UINT64(ARM_O_RDWR, VFS_O_RDWR);
    TEST_ASSERT_EQUAL_UINT64(ARM_O_CREAT, VFS_O_CREAT);
    TEST_ASSERT_EQUAL_UINT64(ARM_O_ALLOWED, VFS_O_ALLOWED);

    TEST_ASSERT_EQUAL_UINT64(0ULL, ARM_FD_STDIN);
    TEST_ASSERT_EQUAL_UINT64(1ULL, ARM_FD_STDOUT);
    TEST_ASSERT_EQUAL_UINT64(2ULL, ARM_FD_STDERR);
    TEST_ASSERT_EQUAL_UINT64(3ULL, ARM_FD_FILE_BASE);
    TEST_ASSERT_EQUAL_UINT64(0ULL, ARM_SEEK_SET);

    TEST_ASSERT_EQUAL_UINT64(2ULL, ARM_VFS_METADATA_VERSION);
    TEST_ASSERT_EQUAL_UINT64(64ULL, ARM_DIRENT_NAME_MAX);
    TEST_ASSERT_EQUAL_UINT64(1ULL, ARM_FILE_TYPE_REGULAR);
    TEST_ASSERT_EQUAL_UINT64(2ULL, ARM_FILE_TYPE_DIRECTORY);
    TEST_ASSERT_EQUAL_UINT64(0x01ULL, ARM_FILE_ATTR_READ_ONLY);
    TEST_ASSERT_EQUAL_UINT64(0x02ULL, ARM_FILE_ATTR_HIDDEN);
    TEST_ASSERT_EQUAL_UINT64(0x04ULL, ARM_FILE_ATTR_SYSTEM);
    TEST_ASSERT_EQUAL_UINT64(0x08ULL, ARM_FILE_ATTR_ARCHIVE);
}

void test_syscall_abi_user_exit_codes_match_documented_constants(void) {
    /* sys_kill and lower-EL fault handling expose these exit codes to
     * waiters. They are part of the observable process ABI. */
    TEST_ASSERT_EQUAL_UINT64(0x80ULL, ARM_PROCESS_EXIT_KILLED);
    TEST_ASSERT_EQUAL_UINT64(0xfffffffffffffff0ULL, ARM_PROCESS_EXIT_FAULT);
    TEST_ASSERT_EQUAL_UINT64(ARM_PROCESS_EXIT_KILLED,
                             KERNEL_USER_KILL_EXIT_CODE);
    TEST_ASSERT_EQUAL_UINT64(ARM_PROCESS_EXIT_FAULT,
                             KERNEL_USER_FAULT_EXIT_CODE);
    TEST_ASSERT_TRUE(ARM_PROCESS_EXIT_KILLED != ARM_PROCESS_EXIT_FAULT);

    TEST_ASSERT_EQUAL_UINT64(ARM_PROCESS_UNUSED, PROCESS_UNUSED);
    TEST_ASSERT_EQUAL_UINT64(ARM_PROCESS_READY, PROCESS_READY);
    TEST_ASSERT_EQUAL_UINT64(ARM_PROCESS_RUNNING, PROCESS_RUNNING);
    TEST_ASSERT_EQUAL_UINT64(ARM_PROCESS_BLOCKED, PROCESS_BLOCKED);
    TEST_ASSERT_EQUAL_UINT64(ARM_PROCESS_ZOMBIE, PROCESS_ZOMBIE);

    TEST_ASSERT_EQUAL_UINT64(ARM_VM_PROT_READ, USER_VM_PROT_READ);
    TEST_ASSERT_EQUAL_UINT64(ARM_VM_PROT_WRITE, USER_VM_PROT_WRITE);
    TEST_ASSERT_EQUAL_UINT64(ARM_VM_PROT_EXEC, USER_VM_PROT_EXEC);
    TEST_ASSERT_EQUAL_UINT64(ARM_MAP_SHARED, USER_VM_MAP_SHARED);
    TEST_ASSERT_EQUAL_UINT64(ARM_MAP_FIXED, USER_VM_MAP_FIXED);
}

void test_syscall_abi_user_range_validation_rejects_out_of_region(void) {
    /* Every syscall that takes a user pointer must reject pointers
     * outside the caller's registered regions. The dispatcher delegates
     * to process_user_range_contains; if that function ever grows lax,
     * the syscall ABI loses its isolation promise. */
    process_t process;
    process_init(&process, 4242U, "abi_range");

    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)process_add_user_region(
                                 &process, 0x1000ULL, 0x100ULL));

    TEST_ASSERT_TRUE(process_user_range_contains(&process, 0x1000ULL, 0x100ULL));
    TEST_ASSERT_TRUE(process_user_range_contains(&process, 0x1050ULL, 0x10ULL));

    TEST_ASSERT_TRUE(!process_user_range_contains(&process, 0x0fffULL, 1ULL));
    TEST_ASSERT_TRUE(!process_user_range_contains(&process, 0x1100ULL, 1ULL));
    TEST_ASSERT_TRUE(!process_user_range_contains(&process, 0x10f0ULL, 0x20ULL));

    TEST_ASSERT_TRUE(process_user_range_contains(&process, 0x1000ULL, 0ULL));
    TEST_ASSERT_TRUE(process_user_range_contains(&process, 0x1100ULL, 0ULL));
    TEST_ASSERT_TRUE(process_user_range_contains(&process, 0x0ULL, 0ULL));

    process_release(&process);
}
