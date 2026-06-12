#include <stdlib.h>
#include <stdint.h>

#include "unity/unity.h"
#include "../kernel/mm/pmm.h"

void test_pmm_init_alloc_free_count(void) {
    size_t pages = 16;
    size_t size = pages * PAGE_SIZE;
    void *mem = NULL;
    int rc = posix_memalign(&mem, PAGE_SIZE, size);
    if (rc != 0) TEST_FAIL("posix_memalign failed");
    TEST_ASSERT_NOT_NULL(mem);

    pmm_init((uint64_t)(uintptr_t)mem, size);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)pages, pmm_free_count());

    uint64_t a = pmm_alloc_page();
    TEST_ASSERT_NOT_NULL((void*)(uintptr_t)a);
    if (pmm_free_count() != (uint64_t)(pages - 1)) {
        TEST_FAIL("pmm_free_count() after alloc != expected");
    }

    pmm_free_page(a);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)pages, pmm_free_count());

    free(mem);
}

void test_pmm_reserve_range(void) {
    size_t pages = 8;
    size_t size = pages * PAGE_SIZE;
    void *mem = NULL;
    int rc = posix_memalign(&mem, PAGE_SIZE, size);
    if (rc != 0) TEST_FAIL("posix_memalign failed");
    TEST_ASSERT_NOT_NULL(mem);

    pmm_init((uint64_t)(uintptr_t)mem, size);
    pmm_reserve_range((uint64_t)(uintptr_t)mem, PAGE_SIZE);
    TEST_ASSERT_EQUAL_UINT64(pages - 1, pmm_free_count());

    free(mem);
}

/* main moved to tests/main.c */
