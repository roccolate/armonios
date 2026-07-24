#include "unity/unity.h"

#include <stddef.h>
#include <stdint.h>

#include "include/armonios/abi/syscall_numbers.h"
#include "include/armonios/abi/vfs.h"
#include "libkarm/arena.h"
#include "libkarm/buffer.h"
#include "libkarm/dynamic_string.h"
#include "libkarm/errno.h"
#include "libkarm/file.h"
#include "libkarm/string.h"

#define TEST_PATH "/fat/docs/readme.txt"
#define TEST_FD   3
#define NO_OFFSET ((size_t)-1)

static const uint8_t *read_data;
static size_t read_size;
static size_t read_offset;
static size_t read_limit;
static size_t read_error_after;
static long read_error;
static int read_overreports;

static uint64_t stat_size;
static uint32_t stat_type;
static long stat_status;
static long open_status;
static long close_status;
static unsigned open_calls;
static unsigned close_calls;

static uint8_t write_data[512];
static size_t write_size;
static size_t write_limit;
static size_t write_error_after;
static long write_error;
static int write_zero_progress;
static int write_overreports;

static void reset_io(void) {
    read_data = 0;
    read_size = 0;
    read_offset = 0;
    read_limit = NO_OFFSET;
    read_error_after = NO_OFFSET;
    read_error = KLI_AGAIN;
    read_overreports = 0;

    stat_size = 0;
    stat_type = ARM_FILE_TYPE_REGULAR;
    stat_status = 0;
    open_status = TEST_FD;
    close_status = 0;
    open_calls = 0;
    close_calls = 0;

    memset(write_data, 0, sizeof(write_data));
    write_size = 0;
    write_limit = NO_OFFSET;
    write_error_after = NO_OFFSET;
    write_error = KLI_AGAIN;
    write_zero_progress = 0;
    write_overreports = 0;
}

static void assert_status(long expected, long actual) {
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(int64_t)expected,
                             (uint64_t)(int64_t)actual);
}

long __syscall1(long number, long a0) {
    if ((uint64_t)number != SYS_CLOSE || a0 != TEST_FD) {
        return KLI_INVAL;
    }

    close_calls++;
    return close_status;
}

long __syscall2(long number, long a0, long a1) {
    const char *path = (const char *)(uintptr_t)a0;

    if ((uint64_t)number == SYS_STAT_V2) {
        arm_stat_v2_t *stat = (arm_stat_v2_t *)(uintptr_t)a1;

        if (path == 0 || strcmp(path, TEST_PATH) != 0 || stat == 0 ||
            stat->version != ARM_VFS_METADATA_VERSION ||
            stat->struct_size != sizeof(*stat)) {
            return KLI_INVAL;
        }
        if (stat_status < 0) {
            return stat_status;
        }

        stat->size = stat_size;
        stat->type = stat_type;
        stat->attributes = 0;
        stat->reserved = 0;
        return 0;
    }

    if ((uint64_t)number == SYS_OPEN) {
        if (path == 0 || strcmp(path, TEST_PATH) != 0 ||
            a1 != ARM_O_RDONLY) {
            return KLI_INVAL;
        }

        open_calls++;
        return open_status;
    }

    return KLI_INVAL;
}

long __syscall3(long number, long a0, long a1, long a2) {
    if ((uint64_t)number == SYS_READ) {
        uint8_t *destination = (uint8_t *)(uintptr_t)a1;
        size_t capacity = (size_t)a2;
        size_t count;

        if (a0 != TEST_FD || destination == 0) {
            return KLI_INVAL;
        }
        if (read_overreports) {
            return (long)(capacity + 1U);
        }
        if (read_offset >= read_error_after) {
            return read_error;
        }
        if (read_offset >= read_size) {
            return 0;
        }

        count = read_size - read_offset;
        if (count > capacity) {
            count = capacity;
        }
        if (count > read_limit) {
            count = read_limit;
        }

        memmove(destination, read_data + read_offset, count);
        read_offset += count;
        return (long)count;
    }

    if ((uint64_t)number == SYS_WRITE) {
        const uint8_t *source = (const uint8_t *)(uintptr_t)a1;
        size_t count = (size_t)a2;

        if (source == 0 || write_size > sizeof(write_data)) {
            return KLI_INVAL;
        }
        if (write_overreports) {
            return (long)(count + 1U);
        }
        if (write_zero_progress) {
            return 0;
        }
        if (write_size >= write_error_after) {
            return write_error;
        }
        if (count > write_limit) {
            count = write_limit;
        }
        if (count > sizeof(write_data) - write_size) {
            return KLI_INVAL;
        }

        memmove(write_data + write_size, source, count);
        write_size += count;
        return (long)count;
    }

    return KLI_INVAL;
}

void test_libkarm_fd_write_all_handles_partial_writes(void) {
    static const uint8_t input[] = {'A', 'r', 'm', 'o', 'n', 'i', 'O', 'S'};

    reset_io();
    write_limit = 2;

    assert_status(0, kli_fd_write_all(ARM_FD_STDOUT, input, sizeof(input)));
    TEST_ASSERT_EQUAL_UINT64(sizeof(input), write_size);
    TEST_ASSERT_TRUE(memcmp(write_data, input, sizeof(input)) == 0);
}

void test_libkarm_fd_write_all_rejects_invalid_progress(void) {
    static const uint8_t input[] = {1, 2, 3};

    reset_io();
    assert_status(0, kli_fd_write_all(ARM_FD_STDOUT, 0, 0));
    assert_status(KLI_INVAL,
                  kli_fd_write_all(ARM_FD_STDOUT, 0, sizeof(input)));

    write_zero_progress = 1;
    assert_status(KLI_AGAIN,
                  kli_fd_write_all(ARM_FD_STDOUT, input, sizeof(input)));

    reset_io();
    write_overreports = 1;
    assert_status(KLI_INVAL,
                  kli_fd_write_all(ARM_FD_STDOUT, input, sizeof(input)));
}

void test_libkarm_fd_write_all_propagates_error_after_partial_write(void) {
    static const uint8_t input[] = {1, 2, 3, 4, 5};

    reset_io();
    write_limit = 2;
    write_error_after = 2;
    write_error = KLI_BADF;

    assert_status(KLI_BADF,
                  kli_fd_write_all(ARM_FD_STDOUT, input, sizeof(input)));
    TEST_ASSERT_EQUAL_UINT64(2, write_size);
}

void test_libkarm_file_read_all_handles_partial_reads(void) {
    static const uint8_t input[] = {'h', 'e', 'l', 'l', 'o'};
    kli_arena_t arena;
    kli_buffer_t output;
    uint8_t storage[256];

    reset_io();
    read_data = input;
    read_size = sizeof(input);
    read_limit = 2;
    stat_size = sizeof(input);

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_file_read_all(TEST_PATH, &arena, &output));

    TEST_ASSERT_EQUAL_UINT64(sizeof(input), output.length);
    TEST_ASSERT_TRUE(memcmp(output.data, input, sizeof(input)) == 0);
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)&arena, (uintptr_t)output.arena);
    TEST_ASSERT_EQUAL_UINT64(1, open_calls);
    TEST_ASSERT_EQUAL_UINT64(1, close_calls);
}

void test_libkarm_file_read_all_handles_growth_after_stat(void) {
    uint8_t input[96];
    kli_arena_t arena;
    kli_buffer_t output;
    uint8_t storage[512];

    for (size_t i = 0; i < sizeof(input); i++) {
        input[i] = (uint8_t)i;
    }

    reset_io();
    read_data = input;
    read_size = sizeof(input);
    read_limit = 17;
    stat_size = 4;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_file_read_all(TEST_PATH, &arena, &output));

    TEST_ASSERT_EQUAL_UINT64(sizeof(input), output.length);
    TEST_ASSERT_TRUE(output.capacity >= sizeof(input));
    TEST_ASSERT_TRUE(memcmp(output.data, input, sizeof(input)) == 0);
}

void test_libkarm_file_read_all_supports_empty_files(void) {
    kli_arena_t arena;
    kli_buffer_t output;
    uint8_t storage[64];

    reset_io();
    stat_size = 0;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_file_read_all(TEST_PATH, &arena, &output));

    TEST_ASSERT_NULL(output.data);
    TEST_ASSERT_EQUAL_UINT64(0, output.length);
    TEST_ASSERT_EQUAL_UINT64(0, output.capacity);
    TEST_ASSERT_EQUAL_UINT64((uintptr_t)&arena, (uintptr_t)output.arena);
}

void test_libkarm_file_read_all_rejects_directory_before_open(void) {
    kli_arena_t arena;
    kli_buffer_t output;
    uint8_t storage[64];
    size_t arena_offset;

    reset_io();
    stat_type = ARM_FILE_TYPE_DIRECTORY;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_NOT_NULL(kli_arena_alloc(&arena, 8));
    arena_offset = arena.offset;
    output.data = (uint8_t *)(uintptr_t)1;
    output.length = 1;
    output.capacity = 1;
    output.arena = &arena;

    assert_status(KLI_INVAL,
                  kli_file_read_all(TEST_PATH, &arena, &output));
    TEST_ASSERT_EQUAL_UINT64(arena_offset, arena.offset);
    TEST_ASSERT_EQUAL_UINT64(0, open_calls);
    TEST_ASSERT_NULL(output.data);
    TEST_ASSERT_NULL(output.arena);
}

void test_libkarm_file_read_all_rolls_back_on_read_error(void) {
    static const uint8_t input[] = {1, 2, 3, 4, 5, 6};
    kli_arena_t arena;
    kli_buffer_t output;
    uint8_t storage[256];
    size_t arena_offset;

    reset_io();
    read_data = input;
    read_size = sizeof(input);
    read_limit = 2;
    read_error_after = 2;
    read_error = KLI_AGAIN;
    stat_size = sizeof(input);

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_NOT_NULL(kli_arena_alloc(&arena, 16));
    arena_offset = arena.offset;

    assert_status(KLI_AGAIN,
                  kli_file_read_all(TEST_PATH, &arena, &output));
    TEST_ASSERT_EQUAL_UINT64(arena_offset, arena.offset);
    TEST_ASSERT_EQUAL_UINT64(1, close_calls);
    TEST_ASSERT_NULL(output.data);
    TEST_ASSERT_NULL(output.arena);
}

void test_libkarm_file_read_all_rolls_back_on_close_or_overreport(void) {
    static const uint8_t input[] = {1, 2, 3};
    kli_arena_t arena;
    kli_buffer_t output;
    uint8_t storage[256];
    size_t arena_offset;

    reset_io();
    read_data = input;
    read_size = sizeof(input);
    stat_size = sizeof(input);
    close_status = KLI_BADF;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    arena_offset = arena.offset;
    assert_status(KLI_BADF,
                  kli_file_read_all(TEST_PATH, &arena, &output));
    TEST_ASSERT_EQUAL_UINT64(arena_offset, arena.offset);
    TEST_ASSERT_NULL(output.data);

    reset_io();
    stat_size = sizeof(input);
    read_overreports = 1;
    assert_status(KLI_INVAL,
                  kli_file_read_all(TEST_PATH, &arena, &output));
    TEST_ASSERT_EQUAL_UINT64(arena_offset, arena.offset);
    TEST_ASSERT_EQUAL_UINT64(1, close_calls);
}

void test_libkarm_file_read_text_builds_terminated_string(void) {
    static const uint8_t input[] = {'A', 'r', 'm', 'o', 'n', 'i', 'O', 'S'};
    kli_arena_t arena;
    kli_string_t output;
    uint8_t storage[256];

    reset_io();
    read_data = input;
    read_size = sizeof(input);
    read_limit = 3;
    stat_size = sizeof(input);

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_file_read_text(TEST_PATH, &arena, &output));

    TEST_ASSERT_EQUAL_UINT64(sizeof(input), kli_string_length(&output));
    TEST_ASSERT_TRUE(strcmp(kli_string_cstr(&output), "ArmoniOS") == 0);
    TEST_ASSERT_EQUAL_UINT64(0,
                             (uint8_t)kli_string_cstr(&output)[sizeof(input)]);
}

void test_libkarm_file_read_text_rejects_embedded_nul_and_rolls_back(void) {
    static const uint8_t input[] = {'a', 'b', '\0', 'c'};
    kli_arena_t arena;
    kli_string_t output;
    uint8_t storage[256];
    size_t arena_offset;

    reset_io();
    read_data = input;
    read_size = sizeof(input);
    stat_size = sizeof(input);

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    TEST_ASSERT_NOT_NULL(kli_arena_alloc(&arena, 8));
    arena_offset = arena.offset;

    assert_status(KLI_INVAL,
                  kli_file_read_text(TEST_PATH, &arena, &output));
    TEST_ASSERT_EQUAL_UINT64(arena_offset, arena.offset);
    TEST_ASSERT_NULL(output.buffer.data);
    TEST_ASSERT_NULL(output.buffer.arena);
    TEST_ASSERT_EQUAL_UINT64(1, close_calls);
}

void test_libkarm_file_read_text_supports_empty_file(void) {
    kli_arena_t arena;
    kli_string_t output;
    uint8_t storage[128];

    reset_io();
    stat_size = 0;

    TEST_ASSERT_EQUAL_UINT64(0, kli_arena_init(&arena, storage, sizeof(storage)));
    assert_status(0, kli_file_read_text(TEST_PATH, &arena, &output));
    TEST_ASSERT_NOT_NULL(kli_string_cstr(&output));
    TEST_ASSERT_EQUAL_UINT64(0, kli_string_length(&output));
    TEST_ASSERT_EQUAL_UINT64(0, (uint8_t)kli_string_cstr(&output)[0]);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_libkarm_fd_write_all_handles_partial_writes);
    RUN_TEST(test_libkarm_fd_write_all_rejects_invalid_progress);
    RUN_TEST(test_libkarm_fd_write_all_propagates_error_after_partial_write);
    RUN_TEST(test_libkarm_file_read_all_handles_partial_reads);
    RUN_TEST(test_libkarm_file_read_all_handles_growth_after_stat);
    RUN_TEST(test_libkarm_file_read_all_supports_empty_files);
    RUN_TEST(test_libkarm_file_read_all_rejects_directory_before_open);
    RUN_TEST(test_libkarm_file_read_all_rolls_back_on_read_error);
    RUN_TEST(test_libkarm_file_read_all_rolls_back_on_close_or_overreport);
    RUN_TEST(test_libkarm_file_read_text_builds_terminated_string);
    RUN_TEST(test_libkarm_file_read_text_rejects_embedded_nul_and_rolls_back);
    RUN_TEST(test_libkarm_file_read_text_supports_empty_file);

    return UNITY_END();
}
