#include "unity/unity.h"
#include "../kernel/init_status.h"

static int test_streq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

void test_init_status_defaults_to_skipped(void) {
    init_status_reset();

    TEST_ASSERT_EQUAL_UINT64(INIT_PHASE_COUNT, init_status_count());
    for (uint32_t i = 0; i < init_status_count(); i++) {
        const init_status_entry_t *entry = init_status_at(i);

        TEST_ASSERT_TRUE(entry != 0);
        TEST_ASSERT_TRUE(entry->name != 0);
        TEST_ASSERT_EQUAL_UINT64(INIT_STATUS_SKIPPED, entry->status);
    }
}

void test_init_status_set_and_get_round_trip(void) {
    const init_status_entry_t *entry;

    init_status_reset();
    init_status_set(INIT_PHASE_STORAGE, INIT_STATUS_WARN);
    init_status_set(INIT_PHASE_DISPLAY, INIT_STATUS_OK);

    TEST_ASSERT_EQUAL_UINT64(INIT_STATUS_WARN,
                             init_status_get(INIT_PHASE_STORAGE));
    TEST_ASSERT_EQUAL_UINT64(INIT_STATUS_OK,
                             init_status_get(INIT_PHASE_DISPLAY));

    entry = init_status_at(INIT_PHASE_STORAGE);
    TEST_ASSERT_TRUE(entry != 0);
    TEST_ASSERT_TRUE(test_streq(entry->name, "storage"));
    TEST_ASSERT_EQUAL_UINT64(INIT_STATUS_WARN, entry->status);
}

void test_init_status_rejects_invalid_phase(void) {
    init_status_reset();
    init_status_set((init_phase_t)INIT_PHASE_COUNT, INIT_STATUS_OK);

    TEST_ASSERT_TRUE(init_status_at(INIT_PHASE_COUNT) == 0);
    TEST_ASSERT_EQUAL_UINT64(INIT_STATUS_FAIL,
                             init_status_get((init_phase_t)INIT_PHASE_COUNT));
}

void test_init_status_labels_are_stable(void) {
    TEST_ASSERT_TRUE(test_streq(init_status_label(INIT_STATUS_OK), "OK"));
    TEST_ASSERT_TRUE(test_streq(init_status_label(INIT_STATUS_WARN), "WARN"));
    TEST_ASSERT_TRUE(test_streq(init_status_label(INIT_STATUS_FAIL), "FAIL"));
    TEST_ASSERT_TRUE(test_streq(init_status_label(INIT_STATUS_SKIPPED),
                                "SKIPPED"));
}
