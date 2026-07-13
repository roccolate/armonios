#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel/mm/pmm.h"
#include "kernel/process.h"
#include "kernel/syscall_helpers.h"

#define TEST_PTE_VALID   (1ULL << 0)
#define TEST_PTE_PAGE    (1ULL << 1)
#define TEST_PTE_AP_USER (1ULL << 6)
#define TEST_PTE_AP_RO   (1ULL << 7)

static uint64_t g_region_start;
static uint64_t g_region_end;
static uint64_t g_page_entries[2];

gui_desktop_t *gui_desktop(void) {
    return 0;
}

int process_user_range_contains(const process_t *process, uint64_t start,
                                uint64_t size) {
    uint64_t end;

    if (process == 0) {
        return 0;
    }
    if (size == 0) {
        return 1;
    }
    if (start > UINT64_MAX - size) {
        return 0;
    }
    end = start + size;
    return start >= g_region_start && end <= g_region_end;
}

uint64_t vmm_leaf_entry(uint64_t *pgd, uint64_t vaddr) {
    uint64_t page;

    if (pgd == 0 || vaddr < g_region_start || vaddr >= g_region_end) {
        return 0;
    }
    page = (vaddr - g_region_start) / PAGE_SIZE;
    return page < 2U ? g_page_entries[page] : 0;
}

static uint64_t user_rw_pte(void) {
    return TEST_PTE_VALID | TEST_PTE_PAGE | TEST_PTE_AP_USER;
}

static uint64_t user_ro_pte(void) {
    return user_rw_pte() | TEST_PTE_AP_RO;
}

static void test_writable_copy_succeeds(process_t *process, uint8_t *pages) {
    const uint8_t source[4] = {1U, 2U, 3U, 4U};
    uint8_t output[4] = {0};
    uint64_t base = (uint64_t)(uintptr_t)pages;

    g_page_entries[0] = user_rw_pte();
    g_page_entries[1] = user_rw_pte();

    assert(sys_copy_to_user(process, base, source, sizeof(source)) == 0);
    assert(pages[0] == 1U && pages[3] == 4U);
    assert(sys_copy_from_user(process, output, base, sizeof(output)) == 0);
    assert(output[0] == 1U && output[3] == 4U);
}

static void test_readonly_output_is_rejected(process_t *process,
                                             uint8_t *pages) {
    const uint8_t source[1] = {0x44U};
    uint64_t address = (uint64_t)(uintptr_t)(pages + PAGE_SIZE);

    g_page_entries[1] = user_ro_pte();
    pages[PAGE_SIZE] = 0x5aU;

    assert(sys_user_buf_in(process, address, 1U) == 0);
    assert(sys_user_buf_out(process, address, 1U) == ERR_PERM);
    assert(sys_copy_to_user(process, address, source, 1U) == ERR_PERM);
    assert(pages[PAGE_SIZE] == 0x5aU);
}

static void test_mixed_range_is_atomic(process_t *process, uint8_t *pages) {
    const uint8_t source[4] = {9U, 8U, 7U, 6U};
    uint64_t address = (uint64_t)(uintptr_t)(pages + PAGE_SIZE - 2U);

    g_page_entries[0] = user_rw_pte();
    g_page_entries[1] = user_ro_pte();
    pages[PAGE_SIZE - 2U] = 0xa1U;
    pages[PAGE_SIZE - 1U] = 0xa2U;
    pages[PAGE_SIZE] = 0xb1U;
    pages[PAGE_SIZE + 1U] = 0xb2U;

    assert(sys_copy_to_user(process, address, source, sizeof(source)) ==
           ERR_PERM);
    assert(pages[PAGE_SIZE - 2U] == 0xa1U);
    assert(pages[PAGE_SIZE - 1U] == 0xa2U);
    assert(pages[PAGE_SIZE] == 0xb1U);
    assert(pages[PAGE_SIZE + 1U] == 0xb2U);
}

static void test_non_user_and_missing_ptes_fail(process_t *process,
                                                uint8_t *pages) {
    uint64_t base = (uint64_t)(uintptr_t)pages;

    g_page_entries[0] = TEST_PTE_VALID | TEST_PTE_PAGE;
    assert(sys_user_buf_in(process, base, 1U) == ERR_INVAL);

    g_page_entries[0] = 0;
    assert(sys_user_buf_out(process, base, 1U) == ERR_INVAL);

    assert(sys_user_buf_in(process, 0, 0) == 0);
}

int main(void) {
    process_t process = {0};
    uint8_t *pages = 0;

    assert(posix_memalign((void **)&pages, PAGE_SIZE, 2U * PAGE_SIZE) == 0);
    for (uint64_t i = 0; i < 2U * PAGE_SIZE; i++) {
        pages[i] = 0;
    }

    g_region_start = (uint64_t)(uintptr_t)pages;
    g_region_end = g_region_start + 2U * PAGE_SIZE;
    process.pid = 55U;
    process.page_table = (uint64_t *)(uintptr_t)1U;

    test_writable_copy_succeeds(&process, pages);
    test_readonly_output_is_rejected(&process, pages);
    test_mixed_range_is_atomic(&process, pages);
    test_non_user_and_missing_ptes_fail(&process, pages);

    free(pages);
    puts("PASS: user-copy permission boundary");
    return 0;
}
