#ifndef UNITY_MINI_H
#define UNITY_MINI_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int UnityBegin(void);
int UnityEnd(void);
void UnityFail(const char *msg, const char *file, int line);

#define UNITY_BEGIN() UnityBegin()
#define UNITY_END() UnityEnd()

#define RUN_TEST(func) do { \
    printf("RUN: %s\n", #func); \
    fflush(stdout); \
    func(); \
} while(0)

#define TEST_FAIL(msg) UnityFail(msg, __FILE__, __LINE__)

#define TEST_ASSERT_NOT_NULL(ptr) do { if ((ptr) == NULL) TEST_FAIL(#ptr " is NULL"); } while(0)
#define TEST_ASSERT_NULL(ptr) do { if ((ptr) != NULL) TEST_FAIL(#ptr " is not NULL"); } while(0)
#define TEST_ASSERT_EQUAL_UINT64(expected, actual) do { if ((uint64_t)(expected) != (uint64_t)(actual)) TEST_FAIL(#actual " != expected"); } while(0)
#define TEST_ASSERT_TRUE(expr) do { if (!(expr)) TEST_FAIL(#expr " is false"); } while(0)

#endif
