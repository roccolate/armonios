#include <stdint.h>

#include "../kernel/bootfs.h"
#include "../kernel/process.h"
#include "../kernel/user_image.h"
#include "../kernel/vfs.h"

#define TEST_IMAGE_SIZE 96U

static _Alignas(8) uint8_t g_source[TEST_IMAGE_SIZE];
static uint64_t g_metadata_size;
static uint32_t g_metadata_type;
static uint64_t g_chunk_limit;
static uint32_t g_read_calls;
static uint8_t g_zero_progress;
static uint8_t g_overreport;

static void check_true(int ok) {
    if (!ok) {
        __builtin_trap();
    }
}

#define CHECK_TRUE(expr) check_true((expr) != 0)
#define CHECK_EQ(expected, actual) check_true((expected) == (actual))

static void clear_bytes(uint8_t *bytes, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        bytes[i] = 0U;
    }
}

static void prepare_valid_image(void) {
    user_flat_image_header_t *header =
        (user_flat_image_header_t *)(void *)g_source;

    clear_bytes(g_source, sizeof(g_source));
    header->magic = USER_IMAGE_MAGIC;
    header->header_size = USER_IMAGE_HEADER_SIZE;
    header->entry_count = 1U;
    header->image_size = sizeof(g_source);
    header->entry_offsets[0] = USER_IMAGE_HEADER_SIZE;
    for (uint32_t i = USER_IMAGE_HEADER_SIZE; i < sizeof(g_source); i++) {
        g_source[i] = (uint8_t)(0x80U + i);
    }

    g_metadata_size = sizeof(g_source);
    g_metadata_type = VFS_FILE_TYPE_REGULAR;
    g_chunk_limit = sizeof(g_source);
    g_read_calls = 0U;
    g_zero_progress = 0U;
    g_overreport = 0U;
}

const bootfs_file_t *bootfs_find(const char *name) {
    (void)name;
    return 0;
}

int vfs_metadata(const char *path, vfs_metadata_t *metadata) {
    if (path == 0 || metadata == 0) {
        return -1;
    }
    metadata->size = g_metadata_size;
    metadata->type = g_metadata_type;
    metadata->attributes = 0U;
    return 0;
}

int vfs_read(const char *path, uint64_t offset, uint8_t *buffer,
             uint64_t capacity, uint64_t *bytes_read) {
    uint64_t count;

    if (bytes_read != 0) {
        *bytes_read = 0U;
    }
    if (path == 0 || buffer == 0 || bytes_read == 0 ||
        offset > g_metadata_size) {
        return -1;
    }

    g_read_calls++;
    if (g_zero_progress != 0U) {
        return 0;
    }
    if (g_overreport != 0U) {
        *bytes_read = capacity + 1U;
        return 0;
    }
    if (offset == g_metadata_size || capacity == 0U) {
        return 0;
    }

    count = g_metadata_size - offset;
    if (count > capacity) {
        count = capacity;
    }
    if (count > g_chunk_limit) {
        count = g_chunk_limit;
    }
    for (uint64_t i = 0; i < count; i++) {
        buffer[i] = g_source[offset + i];
    }
    *bytes_read = count;
    return 0;
}

void process_set_entry(process_t *process, uint64_t pc, uint64_t sp,
                       uint64_t pstate) {
    if (process != 0) {
        process->pc = pc;
        process->sp = sp;
        process->pstate = pstate;
    }
}

int process_add_user_region(process_t *process, uint64_t start, uint64_t size) {
    (void)process;
    (void)start;
    (void)size;
    return 0;
}

int process_remove_user_region(process_t *process, uint64_t start,
                               uint64_t size) {
    (void)process;
    (void)start;
    (void)size;
    return 0;
}

static void test_fragmented_vfs_image_load(void) {
    uint8_t loaded[128];
    user_image_t image = {0};

    prepare_valid_image();
    clear_bytes(loaded, sizeof(loaded));
    g_chunk_limit = 7U;

    CHECK_EQ(0, user_image_load_vfs_flat(
                    &image, "HELLO.KLI", "/fat/HELLO.KLI",
                    (uint64_t)(uintptr_t)loaded, sizeof(loaded), 0U));
    CHECK_TRUE(image.name == (const char *)"HELLO.KLI");
    CHECK_EQ((uint64_t)(uintptr_t)loaded, image.base);
    CHECK_EQ(sizeof(g_source), image.size);
    CHECK_EQ(USER_IMAGE_HEADER_SIZE, image.entry_offset);
    CHECK_TRUE(g_read_calls > 1U);
    for (uint32_t i = 0; i < sizeof(g_source); i++) {
        CHECK_EQ(g_source[i], loaded[i]);
    }
}

static void test_metadata_rejections_happen_before_read(void) {
    uint8_t loaded[128];
    user_image_t image = {0};

    prepare_valid_image();
    g_metadata_type = VFS_FILE_TYPE_DIRECTORY;
    CHECK_EQ(-1, user_image_load_vfs_flat(
                     &image, "DIR", "/fat/DIR",
                     (uint64_t)(uintptr_t)loaded, sizeof(loaded), 0U));
    CHECK_EQ(0U, g_read_calls);

    prepare_valid_image();
    g_metadata_size = USER_IMAGE_HEADER_SIZE - 1U;
    CHECK_EQ(-1, user_image_load_vfs_flat(
                     &image, "SHORT", "/fat/SHORT.KLI",
                     (uint64_t)(uintptr_t)loaded, sizeof(loaded), 0U));
    CHECK_EQ(0U, g_read_calls);

    prepare_valid_image();
    CHECK_EQ(-1, user_image_load_vfs_flat(
                     &image, "LARGE", "/fat/LARGE.KLI",
                     (uint64_t)(uintptr_t)loaded, sizeof(g_source) - 1U, 0U));
    CHECK_EQ(0U, g_read_calls);
}

static void test_read_protocol_failures_are_rejected(void) {
    uint8_t loaded[128];
    user_image_t image = {0};

    prepare_valid_image();
    g_zero_progress = 1U;
    CHECK_EQ(-1, user_image_load_vfs_flat(
                     &image, "ZERO", "/fat/ZERO.KLI",
                     (uint64_t)(uintptr_t)loaded, sizeof(loaded), 0U));
    CHECK_EQ(1U, g_read_calls);

    prepare_valid_image();
    g_overreport = 1U;
    CHECK_EQ(-1, user_image_load_vfs_flat(
                     &image, "OVER", "/fat/OVER.KLI",
                     (uint64_t)(uintptr_t)loaded, sizeof(loaded), 0U));
    CHECK_EQ(1U, g_read_calls);
}

static void test_file_and_header_size_must_match(void) {
    uint8_t loaded[128];
    user_image_t image = {0};
    user_flat_image_header_t *header;

    prepare_valid_image();
    header = (user_flat_image_header_t *)(void *)g_source;
    header->image_size = sizeof(g_source) - 4U;
    CHECK_EQ(-1, user_image_load_vfs_flat(
                     &image, "TAIL", "/fat/TAIL.KLI",
                     (uint64_t)(uintptr_t)loaded, sizeof(loaded), 0U));
    CHECK_TRUE(g_read_calls > 0U);
}

static void test_entry_must_be_instruction_aligned(void) {
    uint8_t loaded[128];
    user_image_t image = {0};
    user_flat_image_header_t *header;

    prepare_valid_image();
    header = (user_flat_image_header_t *)(void *)g_source;
    header->entry_offsets[0] = USER_IMAGE_HEADER_SIZE + 1U;
    CHECK_EQ(-1, user_image_load_vfs_flat(
                     &image, "ALIGN", "/fat/ALIGN.KLI",
                     (uint64_t)(uintptr_t)loaded, sizeof(loaded), 0U));
}

int main(void) {
    test_fragmented_vfs_image_load();
    test_metadata_rejections_happen_before_read();
    test_read_protocol_failures_are_rejected();
    test_file_and_header_size_must_match();
    test_entry_must_be_instruction_aligned();
    return 0;
}
