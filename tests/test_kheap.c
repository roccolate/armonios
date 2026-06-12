#include <stdlib.h>
#include <stdint.h>

#include "unity/unity.h"
#include "../kernel/mm/pmm.h"
#include "../kernel/mm/kheap.h"

void test_kheap_basic_alloc_free(void) {
    size_t pages = 4;
    size_t size = pages * PAGE_SIZE;
    void *mem = NULL;
    int rc = posix_memalign(&mem, PAGE_SIZE, size);
    if (rc != 0) TEST_FAIL("posix_memalign failed");
    TEST_ASSERT_NOT_NULL(mem);

    pmm_init((uint64_t)(uintptr_t)mem, size);
    kheap_init();

    uint64_t total = kheap_total_bytes();
    TEST_ASSERT_TRUE(total > 0);

    void *p = kmalloc(32);
    TEST_ASSERT_NOT_NULL(p);

    uint64_t free_before = kheap_free_bytes();
    kfree(p);
    uint64_t free_after = kheap_free_bytes();
    TEST_ASSERT_TRUE(free_after >= free_before);

    free(mem);
}

/* main moved to tests/main.c */
