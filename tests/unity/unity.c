#include "unity.h"

static int g_tests_run = 0;
static int g_tests_failed = 0;

int UnityBegin(void) {
    g_tests_run = 0;
    g_tests_failed = 0;
    printf("=== RUNNING TESTS ===\n");
    return 0;
}

int UnityEnd(void) {
    if (g_tests_failed == 0) {
        printf("ALL TESTS PASSED (%d)\n", g_tests_run);
        return 0;
    } else {
        printf("%d TEST(S) FAILED (of %d)\n", g_tests_failed, g_tests_run);
        return g_tests_failed;
    }
}

void UnityFail(const char *msg, const char *file, int line) {
    g_tests_failed++;
    printf("FAIL: %s:%d: %s\n", file, line, msg);
    fflush(stdout);
    exit(1);
}
