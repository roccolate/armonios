#include "unity/unity.h"

#include <stddef.h>
#include <stdint.h>

#include "libkarm/arena.h"
#include "libkarm/buffer.h"
#include "libkarm/errno.h"

static int bytes_equal(const uint8_t *left, const uint8_t *right, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (left[i] != right[i]) {
            return 0;
        }
    }
    return 1;
}

void test_libkarm_buffer_init_rejects_invalid_inputs(void) {
    kli_buffer_t buffer;
    kli_arena_t arena;
    uint8_t storage[32];

    TEST_ASSERT_TRUE(kli_buffer_init(0, &arena) < 0);
    TEST_ASSERT_TRUE(kli_buffer_init(&buffer, 0) < 0);
    TEST_ASSERT_NULL(buffer.data);
    TEST_ASSERT_NULL(buffer.arena);

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_init(&buffer, &arena));
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)&arena, (uintptr_t)buffer.arena);
    TEST_ASSERT_EQUAL_UINT64(0, buffer.length);
    TEST_ASSERT_EQUAL_UINT64(0, buffer.capacity);
}

void test_libkarm_buffer_initial_capacity_uses_minimum_growth_block(void) {
    kli_buffer_t buffer;
    kli_arena_t arena;
    uint8_t storage[128];

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_init_capacity(&buffer, &arena, 8));
    TEST_ASSERT_NOT_NULL(buffer.data);
    TEST_ASSERT_EQUAL_UINT64(64, buffer.capacity);
    TEST_ASSERT_EQUAL_UINT64(64, kli_buffer_remaining(&buffer));
}

void test_libkarm_buffer_append_preserves_bytes(void) {
    static const uint8_t expected[] = {1, 2, 3, 4, 5};
    kli_buffer_t buffer;
    kli_arena_t arena;
    uint8_t storage[128];

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_init(&buffer, &arena));
    TEST_ASSERT_EQUAL_UINT64(0,
                             kli_buffer_append(&buffer, expected,
                                               sizeof(expected)));
    TEST_ASSERT_EQUAL_UINT64(sizeof(expected), buffer.length);
    TEST_ASSERT_TRUE(bytes_equal(buffer.data, expected, sizeof(expected)));
}

void test_libkarm_buffer_growth_copies_existing_data(void) {
    uint8_t first[64];
    uint8_t second[32];
    kli_buffer_t buffer;
    kli_arena_t arena;
    uint8_t storage[256];
    uint8_t *old_data;

    for (size_t i = 0; i < sizeof(first); i++) {
        first[i] = (uint8_t)i;
    }
    for (size_t i = 0; i < sizeof(second); i++) {
        second[i] = (uint8_t)(200U + i);
    }

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_init(&buffer, &arena));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_append(&buffer, first, sizeof(first)));
    old_data = buffer.data;
    TEST_ASSERT_EQUAL_UINT64(0,
                             kli_buffer_append(&buffer, second,
                                               sizeof(second)));

    TEST_ASSERT_TRUE(buffer.data != old_data);
    TEST_ASSERT_EQUAL_UINT64(128, buffer.capacity);
    TEST_ASSERT_EQUAL_UINT64(96, buffer.length);
    TEST_ASSERT_TRUE(bytes_equal(buffer.data, first, sizeof(first)));
    TEST_ASSERT_TRUE(bytes_equal(buffer.data + sizeof(first), second,
                                 sizeof(second)));
}

void test_libkarm_buffer_self_append_is_overlap_safe(void) {
    static const uint8_t input[] = {'A', 'B', 'C', 'D'};
    static const uint8_t expected[] = {'A', 'B', 'C', 'D', 'B', 'C'};
    kli_buffer_t buffer;
    kli_arena_t arena;
    uint8_t storage[128];

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_init(&buffer, &arena));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_append(&buffer, input, sizeof(input)));
    TEST_ASSERT_EQUAL_UINT64(0,
                             kli_buffer_append(&buffer, buffer.data + 1, 2));
    TEST_ASSERT_EQUAL_UINT64(sizeof(expected), buffer.length);
    TEST_ASSERT_TRUE(bytes_equal(buffer.data, expected, sizeof(expected)));
}

void test_libkarm_buffer_append_byte_and_clear_reuse_capacity(void) {
    kli_buffer_t buffer;
    kli_arena_t arena;
    uint8_t storage[128];
    uint8_t *data;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_init(&buffer, &arena));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_append_byte(&buffer, 0x5a));
    data = buffer.data;
    TEST_ASSERT_EQUAL_UINT64(1, buffer.length);
    TEST_ASSERT_EQUAL_UINT64(0x5a, buffer.data[0]);

    kli_buffer_clear(&buffer);
    TEST_ASSERT_EQUAL_UINT64(0, buffer.length);
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)data, (uintptr_t)buffer.data);
    TEST_ASSERT_EQUAL_UINT64(64, kli_buffer_remaining(&buffer));
}

void test_libkarm_buffer_failed_growth_preserves_state(void) {
    uint8_t input[64];
    kli_buffer_t buffer;
    kli_arena_t arena;
    uint8_t storage[80];
    uint8_t *data;
    size_t arena_offset;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_init(&buffer, &arena));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_append(&buffer, input, sizeof(input)));
    data = buffer.data;
    arena_offset = arena.offset;

    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)KLI_NOMEM,
                             (uint64_t)(int64_t)
                                 kli_buffer_append_byte(&buffer, 1));
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)data, (uintptr_t)buffer.data);
    TEST_ASSERT_EQUAL_UINT64(64, buffer.length);
    TEST_ASSERT_EQUAL_UINT64(64, buffer.capacity);
    TEST_ASSERT_EQUAL_UINT64(arena_offset, arena.offset);
}

void test_libkarm_buffer_rejects_length_overflow(void) {
    kli_buffer_t buffer;
    kli_arena_t arena;
    uint8_t storage[64];
    uint8_t value = 1;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_init(&buffer, &arena));
    buffer.data = storage;
    buffer.capacity = SIZE_MAX;
    buffer.length = SIZE_MAX;

    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)KLI_INVAL,
                             (uint64_t)(int64_t)
                                 kli_buffer_append(&buffer, &value, 1));
    TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, buffer.length);
}

void test_libkarm_buffer_rejects_inconsistent_storage_state(void) {
    kli_buffer_t buffer;
    kli_arena_t arena;
    uint8_t storage[128];

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_init(&buffer, &arena));

    buffer.capacity = 64;
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)KLI_INVAL,
                             (uint64_t)(int64_t)
                                 kli_buffer_append_byte(&buffer, 1));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_remaining(&buffer));

    buffer.data = storage;
    buffer.capacity = 0;
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)KLI_INVAL,
                             (uint64_t)(int64_t)
                                 kli_buffer_reserve(&buffer, 1));
}

void test_libkarm_buffer_zero_append_accepts_null_data(void) {
    kli_buffer_t buffer;
    kli_arena_t arena;
    uint8_t storage[32];

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_init(&buffer, &arena));
    TEST_ASSERT_EQUAL_UINT64(0, kli_buffer_append(&buffer, 0, 0));
    TEST_ASSERT_EQUAL_UINT64(0, buffer.length);
    TEST_ASSERT_NULL(buffer.data);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_libkarm_buffer_init_rejects_invalid_inputs);
    RUN_TEST(test_libkarm_buffer_initial_capacity_uses_minimum_growth_block);
    RUN_TEST(test_libkarm_buffer_append_preserves_bytes);
    RUN_TEST(test_libkarm_buffer_growth_copies_existing_data);
    RUN_TEST(test_libkarm_buffer_self_append_is_overlap_safe);
    RUN_TEST(test_libkarm_buffer_append_byte_and_clear_reuse_capacity);
    RUN_TEST(test_libkarm_buffer_failed_growth_preserves_state);
    RUN_TEST(test_libkarm_buffer_rejects_length_overflow);
    RUN_TEST(test_libkarm_buffer_rejects_inconsistent_storage_state);
    RUN_TEST(test_libkarm_buffer_zero_append_accepts_null_data);

    return UNITY_END();
}
