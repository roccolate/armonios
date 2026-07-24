#include "unity/unity.h"

#include <stddef.h>
#include <stdint.h>

#include "libkarm/arena.h"
#include "libkarm/dynamic_string.h"
#include "libkarm/errno.h"
#include "libkarm/string.h"

static void assert_status(long expected, long actual) {
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)expected,
                             (uint64_t)(int64_t)actual);
}

void test_libkarm_string_init_rejects_invalid_inputs_and_builds_empty_cstr(void) {
    kli_string_t string;
    kli_arena_t arena;
    uint8_t storage[128];

    assert_status(KLI_INVAL, kli_string_init(0, &arena));
    assert_status(KLI_INVAL, kli_string_init(&string, 0));
    TEST_ASSERT_NULL(string.buffer.data);
    TEST_ASSERT_NULL(string.buffer.arena);

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_string_init(&string, &arena));
    TEST_ASSERT_NOT_NULL(kli_string_cstr(&string));
    TEST_ASSERT_EQUAL_UINT64(0, kli_string_length(&string));
    TEST_ASSERT_EQUAL_UINT64(0, (uint8_t)kli_string_cstr(&string)[0]);
    TEST_ASSERT_TRUE(kli_string_capacity(&string) >= 1U);
}

void test_libkarm_string_init_capacity_reserves_text_plus_terminator(void) {
    kli_string_t string;
    kli_arena_t arena;
    uint8_t storage[256];

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_string_init_capacity(&string, &arena, 80));
    TEST_ASSERT_TRUE(kli_string_capacity(&string) >= 80U);
    TEST_ASSERT_EQUAL_UINT64(0, kli_string_length(&string));
    TEST_ASSERT_EQUAL_UINT64(0, (uint8_t)kli_string_cstr(&string)[0]);

    assert_status(KLI_INVAL,
                  kli_string_init_capacity(&string, &arena, SIZE_MAX));
    TEST_ASSERT_NULL(string.buffer.data);
    TEST_ASSERT_NULL(string.buffer.arena);
}

void test_libkarm_string_assign_tracks_length_and_terminator(void) {
    kli_string_t string;
    kli_arena_t arena;
    uint8_t storage[128];

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_string_init(&string, &arena));
    assert_status(0, kli_string_assign(&string, "ArmoniOS"));

    TEST_ASSERT_EQUAL_UINT64(8, kli_string_length(&string));
    TEST_ASSERT_TRUE(strcmp(kli_string_cstr(&string), "ArmoniOS") == 0);
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint8_t)kli_string_cstr(&string)[8]);
}

void test_libkarm_string_assign_n_rejects_embedded_nul_without_changes(void) {
    static const char invalid_text[3] = {'a', '\0', 'b'};
    kli_string_t string;
    kli_arena_t arena;
    uint8_t storage[128];
    const char *before;
    size_t arena_offset;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_string_init(&string, &arena));
    assert_status(0, kli_string_assign(&string, "valid"));
    before = kli_string_cstr(&string);
    arena_offset = arena.offset;

    assert_status(KLI_INVAL,
                  kli_string_assign_n(&string, invalid_text,
                                      sizeof(invalid_text)));
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)before,
                             (uintptr_t)kli_string_cstr(&string));
    TEST_ASSERT_TRUE(strcmp(kli_string_cstr(&string), "valid") == 0);
    TEST_ASSERT_EQUAL_UINT64(arena_offset, arena.offset);
}

void test_libkarm_string_append_and_append_char_preserve_cstr(void) {
    kli_string_t string;
    kli_arena_t arena;
    uint8_t storage[256];

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_string_init(&string, &arena));
    assert_status(0, kli_string_assign(&string, "ArmoniOS"));
    assert_status(0, kli_string_append(&string, " BASIC"));
    assert_status(0, kli_string_append_char(&string, '!'));

    TEST_ASSERT_TRUE(strcmp(kli_string_cstr(&string),
                            "ArmoniOS BASIC!") == 0);
    TEST_ASSERT_EQUAL_UINT64(15, kli_string_length(&string));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint8_t)kli_string_cstr(&string)[15]);
    assert_status(KLI_INVAL, kli_string_append_char(&string, '\0'));
}

void test_libkarm_string_growth_copies_text_and_keeps_terminator(void) {
    char text[80];
    kli_string_t string;
    kli_arena_t arena;
    uint8_t storage[512];
    const char *old_data;

    for (size_t i = 0; i < sizeof(text) - 1U; i++) {
        text[i] = 'x';
    }
    text[sizeof(text) - 1U] = '\0';

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_string_init(&string, &arena));
    old_data = kli_string_cstr(&string);
    assert_status(0, kli_string_assign(&string, text));

    TEST_ASSERT_TRUE(kli_string_cstr(&string) != old_data);
    TEST_ASSERT_EQUAL_UINT64(sizeof(text) - 1U,
                             kli_string_length(&string));
    TEST_ASSERT_TRUE(strcmp(kli_string_cstr(&string), text) == 0);
    TEST_ASSERT_EQUAL_UINT64(
        0, (uint8_t)kli_string_cstr(&string)[sizeof(text) - 1U]);
}

void test_libkarm_string_self_append_survives_arena_growth(void) {
    char text[64];
    kli_string_t string;
    kli_arena_t arena;
    uint8_t storage[512];
    const char *old_data;

    for (size_t i = 0; i < sizeof(text) - 1U; i++) {
        text[i] = 'a';
    }
    text[sizeof(text) - 1U] = '\0';

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_string_init(&string, &arena));
    assert_status(0, kli_string_assign(&string, text));
    old_data = kli_string_cstr(&string);

    assert_status(0,
                  kli_string_append_n(&string, kli_string_cstr(&string), 2));
    TEST_ASSERT_TRUE(kli_string_cstr(&string) != old_data);
    TEST_ASSERT_EQUAL_UINT64(65, kli_string_length(&string));
    TEST_ASSERT_EQUAL_UINT64('a',
                             (uint8_t)kli_string_cstr(&string)[63]);
    TEST_ASSERT_EQUAL_UINT64('a',
                             (uint8_t)kli_string_cstr(&string)[64]);
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint8_t)kli_string_cstr(&string)[65]);
}

void test_libkarm_string_clear_reuses_storage_and_capacity(void) {
    kli_string_t string;
    kli_arena_t arena;
    uint8_t storage[128];
    const char *data;
    size_t capacity;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_string_init(&string, &arena));
    assert_status(0, kli_string_assign(&string, "temporary"));
    data = kli_string_cstr(&string);
    capacity = kli_string_capacity(&string);

    kli_string_clear(&string);
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)data,
                             (uintptr_t)kli_string_cstr(&string));
    TEST_ASSERT_EQUAL_UINT64(capacity, kli_string_capacity(&string));
    TEST_ASSERT_EQUAL_UINT64(0, kli_string_length(&string));
    TEST_ASSERT_EQUAL_UINT64(0, (uint8_t)kli_string_cstr(&string)[0]);
}

void test_libkarm_string_failed_growth_preserves_text_and_arena_offset(void) {
    char text[63];
    kli_string_t string;
    kli_arena_t arena;
    uint8_t storage[80];
    const char *data;
    size_t arena_offset;

    for (size_t i = 0; i < sizeof(text); i++) {
        text[i] = 'z';
    }

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_string_init(&string, &arena));
    assert_status(0, kli_string_assign_n(&string, text, sizeof(text)));
    data = kli_string_cstr(&string);
    arena_offset = arena.offset;

    assert_status(KLI_NOMEM, kli_string_append_char(&string, '!'));
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)data,
                             (uintptr_t)kli_string_cstr(&string));
    TEST_ASSERT_EQUAL_UINT64(sizeof(text), kli_string_length(&string));
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint8_t)kli_string_cstr(&string)[sizeof(text)]);
    TEST_ASSERT_EQUAL_UINT64(arena_offset, arena.offset);
}

void test_libkarm_string_rejects_invalid_text_and_invalidated_storage(void) {
    kli_string_t string;
    kli_arena_t arena;
    uint8_t storage[128];

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_string_init(&string, &arena));
    assert_status(KLI_INVAL, kli_string_assign(&string, 0));
    assert_status(KLI_INVAL, kli_string_append(&string, 0));
    assert_status(0, kli_string_append_n(&string, 0, 0));

    string.buffer.data[string.buffer.length] = 'x';
    TEST_ASSERT_NULL(kli_string_cstr(&string));
    assert_status(KLI_INVAL, kli_string_append(&string, "x"));

    string.buffer.data[string.buffer.length] = 0;
    kli_arena_reset(&arena);
    TEST_ASSERT_NULL(kli_string_cstr(&string));
    TEST_ASSERT_EQUAL_UINT64(0, kli_string_length(&string));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_libkarm_string_init_rejects_invalid_inputs_and_builds_empty_cstr);
    RUN_TEST(test_libkarm_string_init_capacity_reserves_text_plus_terminator);
    RUN_TEST(test_libkarm_string_assign_tracks_length_and_terminator);
    RUN_TEST(test_libkarm_string_assign_n_rejects_embedded_nul_without_changes);
    RUN_TEST(test_libkarm_string_append_and_append_char_preserve_cstr);
    RUN_TEST(test_libkarm_string_growth_copies_text_and_keeps_terminator);
    RUN_TEST(test_libkarm_string_self_append_survives_arena_growth);
    RUN_TEST(test_libkarm_string_clear_reuses_storage_and_capacity);
    RUN_TEST(test_libkarm_string_failed_growth_preserves_text_and_arena_offset);
    RUN_TEST(test_libkarm_string_rejects_invalid_text_and_invalidated_storage);

    return UNITY_END();
}
