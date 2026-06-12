#include "unity/unity.h"

/* Declare test functions */
void test_pmm_init_alloc_free_count(void);
void test_pmm_reserve_range(void);
void test_kheap_basic_alloc_free(void);

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_pmm_init_alloc_free_count);
    RUN_TEST(test_pmm_reserve_range);
    RUN_TEST(test_kheap_basic_alloc_free);

    return UNITY_END();
}
