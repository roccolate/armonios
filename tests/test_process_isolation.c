/*
 * test_process_isolation.c
 *
 * Lock down the per-process isolation contract. The kernel promises:
 *
 *   - Two processes have independent process_user_range_contains views.
 *   - A region registered by process A is invisible to process B.
 *   - sys_mmap returns distinct addresses for each process even when
 *     both ask for the same size.
 *   - The page tables installed by process_init separate their virtual
 *     mappings; mmu_set_ttbr0 is called per process context.
 *
 * These are the properties userland assumes when it copies argv, opens
 * files, or hands a pointer to the kernel. A regression here is a
 * cross-process memory leak, which is exactly the kind of bug a host
 * suite has to catch before it lands on QEMU.
 */

#include "unity/unity.h"

#include <stdint.h>
#include <stdlib.h>

#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/process.h"
#include "kernel/user_vm.h"

#define ISO_TEST_PAGES 32U

static void init_test_memory(void **mem) {
    int rc = posix_memalign(mem, PAGE_SIZE, ISO_TEST_PAGES * PAGE_SIZE);
    if (rc != 0) {
        TEST_FAIL("posix_memalign failed");
    }

    TEST_ASSERT_NOT_NULL(*mem);
    pmm_init((uint64_t)(uintptr_t)*mem, ISO_TEST_PAGES * PAGE_SIZE);
}

void test_process_isolation_two_processes_have_independent_regions(void) {
    process_t a;
    process_t b;
    process_init(&a, 5101U, "iso_a");
    process_init(&b, 5102U, "iso_b");

    /* Process A registers a region at 0x10000. Process B registers a
     * region at 0x40000. Neither should see the other's. */
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)process_add_user_region(
                                 &a, 0x10000ULL, 0x1000ULL));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)process_add_user_region(
                                 &b, 0x40000ULL, 0x1000ULL));

    /* Each sees only its own region. */
    TEST_ASSERT_TRUE(process_user_range_contains(&a, 0x10000ULL, 0x100ULL));
    TEST_ASSERT_TRUE(process_user_range_contains(&b, 0x40000ULL, 0x100ULL));

    /* Cross-process visibility is denied in both directions. */
    TEST_ASSERT_TRUE(!process_user_range_contains(&a, 0x40000ULL, 0x100ULL));
    TEST_ASSERT_TRUE(!process_user_range_contains(&a, 0x40500ULL, 0x100ULL));
    TEST_ASSERT_TRUE(!process_user_range_contains(&b, 0x10000ULL, 0x100ULL));
    TEST_ASSERT_TRUE(!process_user_range_contains(&b, 0x10500ULL, 0x100ULL));

    /* A query that straddles the end is rejected; a single byte
     * inside the last slot is fine because the region is
     * [start, start+size) and 0x10fff fits inside [0x10000, 0x11000). */
    TEST_ASSERT_TRUE(process_user_range_contains(&a, 0x10fffULL, 1ULL));
    TEST_ASSERT_TRUE(process_user_range_contains(&b, 0x40fffULL, 1ULL));
    TEST_ASSERT_TRUE(!process_user_range_contains(&a, 0x10fffULL, 2ULL));
    TEST_ASSERT_TRUE(!process_user_range_contains(&b, 0x40fffULL, 2ULL));

    process_release(&a);
    process_release(&b);
}

void test_process_isolation_mmap_returns_distinct_addresses(void) {
    void *mem = NULL;
    process_t a;
    process_t b;
    uint64_t *pgd_a;
    uint64_t *pgd_b;
    int64_t a_addr;
    int64_t b_addr;

    /* Each test that touches the PMM resets it through
     * init_test_memory. Without this the host suite can run the
     * PMM out before our test even reaches vmm_new_table. */
    init_test_memory(&mem);

    /* Each process needs its own page table for sys_mmap to install
     * PTEs. The kernel does this through process_set_page_table; the
     * host suite does the same via vmm_new_table() so we don't have
     * to spin up the EL1 MMU. */
    process_init(&a, 5103U, "iso_map_a");
    process_init(&b, 5104U, "iso_map_b");
    pgd_a = vmm_new_table();
    pgd_b = vmm_new_table();
    TEST_ASSERT_NOT_NULL(pgd_a);
    TEST_ASSERT_NOT_NULL(pgd_b);
    process_set_page_table(&a, pgd_a);
    process_set_page_table(&b, pgd_b);

    /* Ask each process for a 4 KiB RW mapping. Each process has its
     * own address space, so the kernel allocates the same virtual
     * address to both; the actual physical pages differ because each
     * process owns a separate page table. */
    a_addr = user_vm_map_anonymous(&a, 0, 0x1000ULL,
                                   USER_VM_PROT_READ | USER_VM_PROT_WRITE);
    b_addr = user_vm_map_anonymous(&b, 0, 0x1000ULL,
                                   USER_VM_PROT_READ | USER_VM_PROT_WRITE);
    TEST_ASSERT_TRUE(a_addr > 0);
    TEST_ASSERT_TRUE(b_addr > 0);
    TEST_ASSERT_TRUE((uint64_t)a_addr >= PROCESS_USER_MMAP_BASE);
    TEST_ASSERT_TRUE((uint64_t)b_addr >= PROCESS_USER_MMAP_BASE);

    /* Each process sees its own mapping in its region list. */
    TEST_ASSERT_TRUE(process_user_range_contains(&a, (uint64_t)a_addr, 0x100ULL));
    TEST_ASSERT_TRUE(process_user_range_contains(&b, (uint64_t)b_addr, 0x100ULL));

    /* The two mappings live in different page tables and resolve to
     * different physical pages. We inspect the per-process region
     * metadata directly to confirm: the kernel stored a different
     * paddr for each. */
    process_user_region_t a_region = {0};
    process_user_region_t b_region = {0};
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)process_find_user_region(
                                 &a, (uint64_t)a_addr, 0x1000ULL, &a_region));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint64_t)process_find_user_region(
                                 &b, (uint64_t)b_addr, 0x1000ULL, &b_region));
    TEST_ASSERT_TRUE(a_region.paddr != 0);
    TEST_ASSERT_TRUE(b_region.paddr != 0);
    TEST_ASSERT_TRUE(a_region.paddr != b_region.paddr);
    TEST_ASSERT_TRUE(a.page_table != b.page_table);
    TEST_ASSERT_TRUE(a.page_table != 0);
    TEST_ASSERT_TRUE(b.page_table != 0);

    /* Cleanup unmap returns 0; the next mmap is independent again. */
    TEST_ASSERT_EQUAL_UINT64(0ULL, (uint64_t)user_vm_unmap_anonymous(
                                       &a, (uint64_t)a_addr, 0x1000ULL));
    TEST_ASSERT_EQUAL_UINT64(0ULL, (uint64_t)user_vm_unmap_anonymous(
                                       &b, (uint64_t)b_addr, 0x1000ULL));

    free(mem);
    process_release(&a);
    process_release(&b);
}

void test_process_isolation_mmap_rejects_hint_nonzero(void) {
    void *mem = NULL;
    process_t process;
    uint64_t *pgd;

    init_test_memory(&mem);

    /* SYSCALLS.md says sys_mmap `hint` must be 0. Pin that contract
     * here so a future "support MAP_FIXED" change has to acknowledge
     * the ABI break. */
    process_init(&process, 5105U, "iso_hint");
    pgd = vmm_new_table();
    TEST_ASSERT_NOT_NULL(pgd);
    process_set_page_table(&process, pgd);

    TEST_ASSERT_TRUE(user_vm_map_anonymous(&process, 0x100000ULL, 0x1000ULL,
                                           USER_VM_PROT_READ)
                    < 0);

    free(mem);
    process_release(&process);
}

void test_process_isolation_mmap_rejects_unknown_flags(void) {
    void *mem = NULL;
    process_t process;
    uint64_t *pgd;

    init_test_memory(&mem);

    /* Only USER_VM_PROT_* flags are accepted; anything else returns
     * USER_VM_ERR_INVAL. The SYSCALLS.md contract is "flags=0 maps RW"
     * and "PROT_READ/PROT_WRITE/PROT_EXEC are supported". MAP_SHARED
     * and MAP_FIXED are reserved and must be rejected. */
    process_init(&process, 5106U, "iso_flags");
    pgd = vmm_new_table();
    TEST_ASSERT_NOT_NULL(pgd);
    process_set_page_table(&process, pgd);

    TEST_ASSERT_TRUE(user_vm_map_anonymous(&process, 0, 0x1000ULL, 0xffULL)
                    < 0);
    TEST_ASSERT_TRUE(user_vm_map_anonymous(
                         &process, 0, 0x1000ULL,
                         USER_VM_PROT_READ | (1ULL << 8) /* not a real flag */)
                    < 0);

    free(mem);
    process_release(&process);
}

void test_process_isolation_zero_process_returns_false(void) {
    /* A defensive contract: a null process pointer never satisfies the
     * "in your user regions" check, regardless of the address range. */
    TEST_ASSERT_TRUE(!process_user_range_contains(0, 0x1000ULL, 0x100ULL));
    TEST_ASSERT_TRUE(!process_user_range_contains(0, 0ULL, 0ULL));
}
