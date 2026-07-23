#include "unity/unity.h"

#include <stddef.h>
#include <stdint.h>

#include "include/armonios/abi/syscall_numbers.h"
#include "libkarm/arena.h"
#include "libkarm/errno.h"

static long stub_mmap_result;
static long stub_munmap_result;
static long stub_mmap_hint;
static long stub_mmap_size;
static long stub_mmap_flags;
static long stub_munmap_address;
static long stub_munmap_size;

long __syscall2(long number, long a0, long a1) {
    if (number != (long)SYS_MUNMAP) {
        return KLI_INVAL;
    }

    stub_munmap_address = a0;
    stub_munmap_size = a1;
    return stub_munmap_result;
}

long __syscall3(long number, long a0, long a1, long a2) {
    if (number != (long)SYS_MMAP) {
        return KLI_INVAL;
    }

    stub_mmap_hint = a0;
    stub_mmap_size = a1;
    stub_mmap_flags = a2;
    return stub_mmap_result;
}

static void reset_syscall_stubs(void) {
    stub_mmap_result = KLI_NOMEM;
    stub_munmap_result = 0;
    stub_mmap_hint = -1;
    stub_mmap_size = -1;
    stub_mmap_flags = -1;
    stub_munmap_address = -1;
    stub_munmap_size = -1;
}

void test_libkarm_arena_init_rejects_invalid_inputs(void) {
    kli_arena_t arena;
    uint8_t buffer[16];

    TEST_ASSERT_TRUE(kli_arena_init(0, buffer, sizeof(buffer)) != 0);
    TEST_ASSERT_TRUE(kli_arena_init(&arena, 0, sizeof(buffer)) != 0);
    TEST_ASSERT_NULL(arena.base);
    TEST_ASSERT_TRUE(kli_arena_init(&arena, buffer, 0) != 0);
    TEST_ASSERT_NULL(arena.base);
}

void test_libkarm_arena_allocates_and_tracks_remaining_bytes(void) {
    kli_arena_t arena;
    uint8_t buffer[64];
    void *first;
    void *second;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, buffer, sizeof(buffer)));
    first = kli_arena_alloc(&arena, 8);
    second = kli_arena_alloc(&arena, 8);

    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_TRUE((uintptr_t)second > (uintptr_t)first);
    TEST_ASSERT_EQUAL_UINT64(48, kli_arena_remaining(&arena));
}

void test_libkarm_arena_honors_power_of_two_alignment(void) {
    kli_arena_t arena;
    uint8_t buffer[96];
    void *value;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, buffer + 1, 95));
    value = kli_arena_alloc_aligned(&arena, 7, 32);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)value & 31U);
    TEST_ASSERT_TRUE(arena.offset >= 7);
}

void test_libkarm_arena_rejects_invalid_alignment_without_consuming_space(void) {
    kli_arena_t arena;
    uint8_t buffer[32];

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, buffer, sizeof(buffer)));
    TEST_ASSERT_NULL(kli_arena_alloc_aligned(&arena, 4, 3));
    TEST_ASSERT_NULL(kli_arena_alloc_aligned(&arena, 4, 0));
    TEST_ASSERT_EQUAL_UINT64(0, arena.offset);
}

void test_libkarm_arena_exhaustion_preserves_previous_offset(void) {
    kli_arena_t arena;
    uint8_t buffer[24];
    size_t before;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, buffer, sizeof(buffer)));
    TEST_ASSERT_NOT_NULL(kli_arena_alloc_aligned(&arena, 16, 1));
    before = arena.offset;

    TEST_ASSERT_NULL(kli_arena_alloc_aligned(&arena, 16, 1));
    TEST_ASSERT_EQUAL_UINT64(before, arena.offset);
}

void test_libkarm_arena_reset_reuses_storage(void) {
    kli_arena_t arena;
    uint8_t buffer[32];
    void *before;
    void *after;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, buffer, sizeof(buffer)));
    before = kli_arena_alloc_aligned(&arena, 12, 1);
    TEST_ASSERT_NOT_NULL(before);

    kli_arena_reset(&arena);
    TEST_ASSERT_EQUAL_UINT64(sizeof(buffer), kli_arena_remaining(&arena));
    after = kli_arena_alloc_aligned(&arena, 12, 1);
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)before, (uintptr_t)after);
}

void test_libkarm_arena_rejects_pointer_and_size_overflow(void) {
    kli_arena_t arena;

    arena.base = (uint8_t *)(uintptr_t)(UINTPTR_MAX - 3U);
    arena.capacity = 16;
    arena.offset = 8;
    arena.mapping_size = 0;

    TEST_ASSERT_NULL(kli_arena_alloc_aligned(&arena, 1, 8));
    TEST_ASSERT_EQUAL_UINT64(8, arena.offset);

    arena.base = (uint8_t *)(uintptr_t)16;
    arena.capacity = SIZE_MAX;
    arena.offset = SIZE_MAX - 2U;
    TEST_ASSERT_NULL(kli_arena_alloc_aligned(&arena, 8, 1));
    TEST_ASSERT_EQUAL_UINT64(SIZE_MAX - 2U, arena.offset);
}

void test_libkarm_arena_map_propagates_error_and_clears_state(void) {
    kli_arena_t arena;

    arena.base = (uint8_t *)(uintptr_t)1234;
    arena.capacity = 99;
    arena.offset = 4;
    arena.mapping_size = 99;
    reset_syscall_stubs();
    stub_mmap_result = KLI_NOMEM;

    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)KLI_NOMEM,
                             (uint64_t)(int64_t)kli_arena_map(&arena, 4096));
    TEST_ASSERT_NULL(arena.base);
    TEST_ASSERT_EQUAL_UINT64(0, arena.capacity);
    TEST_ASSERT_EQUAL_UINT64(0, arena.mapping_size);
    TEST_ASSERT_EQUAL_UINT64(0, stub_mmap_hint);
    TEST_ASSERT_EQUAL_UINT64(4096, stub_mmap_size);
    TEST_ASSERT_EQUAL_UINT64(0, stub_mmap_flags);
}

void test_libkarm_arena_map_and_destroy_use_exact_mapping_contract(void) {
    kli_arena_t arena;
    uint8_t buffer[64];

    reset_syscall_stubs();
    stub_mmap_result = (long)(uintptr_t)buffer;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_map(&arena, sizeof(buffer)));
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)buffer, (uintptr_t)arena.base);
    TEST_ASSERT_EQUAL_UINT64(sizeof(buffer), arena.capacity);
    TEST_ASSERT_EQUAL_UINT64(sizeof(buffer), arena.mapping_size);
    TEST_ASSERT_NOT_NULL(kli_arena_alloc_aligned(&arena, 8, 8));

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_destroy(&arena));
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)buffer,
                             (uintptr_t)stub_munmap_address);
    TEST_ASSERT_EQUAL_UINT64(sizeof(buffer), stub_munmap_size);
    TEST_ASSERT_NULL(arena.base);
    TEST_ASSERT_EQUAL_UINT64(0, arena.capacity);
    TEST_ASSERT_EQUAL_UINT64(0, arena.offset);
    TEST_ASSERT_EQUAL_UINT64(0, arena.mapping_size);
}

void test_libkarm_arena_destroy_preserves_state_when_unmap_fails(void) {
    kli_arena_t arena;
    uint8_t buffer[32];

    reset_syscall_stubs();
    stub_mmap_result = (long)(uintptr_t)buffer;
    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_map(&arena, sizeof(buffer)));
    stub_munmap_result = KLI_INVAL;

    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)KLI_INVAL,
                             (uint64_t)(int64_t)kli_arena_destroy(&arena));
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)buffer, (uintptr_t)arena.base);
    TEST_ASSERT_EQUAL_UINT64(sizeof(buffer), arena.mapping_size);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_libkarm_arena_init_rejects_invalid_inputs);
    RUN_TEST(test_libkarm_arena_allocates_and_tracks_remaining_bytes);
    RUN_TEST(test_libkarm_arena_honors_power_of_two_alignment);
    RUN_TEST(test_libkarm_arena_rejects_invalid_alignment_without_consuming_space);
    RUN_TEST(test_libkarm_arena_exhaustion_preserves_previous_offset);
    RUN_TEST(test_libkarm_arena_reset_reuses_storage);
    RUN_TEST(test_libkarm_arena_rejects_pointer_and_size_overflow);
    RUN_TEST(test_libkarm_arena_map_propagates_error_and_clears_state);
    RUN_TEST(test_libkarm_arena_map_and_destroy_use_exact_mapping_contract);
    RUN_TEST(test_libkarm_arena_destroy_preserves_state_when_unmap_fails);

    return UNITY_END();
}
