#include <stdint.h>

#include "unity/unity.h"
#include "../kernel/panel_boot_argv.h"

#define TEST_STACK_BASE 0x0000000000800000ULL
#define TEST_STACK_SIZE 4096U

static uint64_t load_u64(const uint8_t *src) {
    uint64_t value = 0;

    for (uint32_t i = 0; i < sizeof(uint64_t); i++) {
        value |= (uint64_t)src[i] << (i * 8U);
    }
    return value;
}

static void clear_stack(uint8_t *stack, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        stack[i] = 0xccU;
    }
}

static int stack_string_equals(const uint8_t *stack, uint64_t vaddr,
                               const char *expected) {
    uint64_t offset = vaddr - TEST_STACK_BASE;
    uint32_t i = 0;

    if (vaddr < TEST_STACK_BASE || offset >= TEST_STACK_SIZE || expected == 0) {
        return 0;
    }
    for (;;) {
        char actual = (char)stack[offset + i];

        if (actual != expected[i]) {
            return 0;
        }
        if (actual == '\0') {
            return 1;
        }
        i++;
        if (offset + i >= TEST_STACK_SIZE) {
            return 0;
        }
    }
}

static void set_args(panel_boot_argv_t *argv, const char *a, const char *b,
                     const char *c) {
    const char *values[3] = {a, b, c};
    uint16_t used = 0U;

    for (uint32_t i = 0; i < 3U; i++) {
        uint16_t length = 0U;

        argv->offsets[i] = used;
        do {
            argv->bytes[used + length] = values[i][length];
        } while (values[i][length++] != '\0');
        used = (uint16_t)(used + length);
    }
    argv->bytes_used = used;
}

void test_panel_boot_argv_rejects_too_many_strings(void) {
    uint8_t stack[TEST_STACK_SIZE] __attribute__((aligned(16)));
    panel_boot_argv_t argv = {0};
    uint64_t argv_vaddr = 0xfeedfaceULL;

    argv.bytes[0] = 'x';
    argv.bytes[1] = '\0';
    argv.bytes_used = 2U;
    TEST_ASSERT_EQUAL_UINT64(
        (uint64_t)-1,
        (uint64_t)panel_boot_place_argv_on_stack(
            stack, TEST_STACK_BASE, TEST_STACK_SIZE, &argv,
            PANEL_BOOT_ARGV_MAX_STRINGS + 1U, &argv_vaddr));
    TEST_ASSERT_EQUAL_UINT64(0xfeedfaceULL, argv_vaddr);
}

void test_panel_boot_argv_rejects_total_string_budget_overflow(void) {
    uint8_t stack[TEST_STACK_SIZE] __attribute__((aligned(16)));
    panel_boot_argv_t argv = {0};
    uint64_t argv_vaddr = 0xfeedfaceULL;

    argv.bytes_used = PANEL_BOOT_ARGV_MAX_BYTES + 1U;
    TEST_ASSERT_EQUAL_UINT64(
        (uint64_t)-1,
        (uint64_t)panel_boot_place_argv_on_stack(
            stack, TEST_STACK_BASE, TEST_STACK_SIZE, &argv, 1U,
            &argv_vaddr));
    TEST_ASSERT_EQUAL_UINT64(0xfeedfaceULL, argv_vaddr);
}

void test_panel_boot_argv_packs_well_formed_args_with_alignment_and_sentinel(void) {
    uint8_t stack[TEST_STACK_SIZE] __attribute__((aligned(16)));
    panel_boot_argv_t argv = {0};
    uint64_t argv_vaddr = 0;
    uint64_t offset;
    uint64_t arg0_vaddr;
    uint64_t arg1_vaddr;
    uint64_t arg2_vaddr;

    clear_stack(stack, sizeof(stack));
    set_args(&argv, "editor", "--safe", "file.txt");

    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)panel_boot_place_argv_on_stack(
               stack, TEST_STACK_BASE, TEST_STACK_SIZE, &argv, 3U,
               &argv_vaddr));
    TEST_ASSERT_TRUE((argv_vaddr & 0xfULL) == 0);
    TEST_ASSERT_TRUE(argv_vaddr >= TEST_STACK_BASE);
    TEST_ASSERT_TRUE(argv_vaddr < TEST_STACK_BASE + TEST_STACK_SIZE);

    offset = argv_vaddr - TEST_STACK_BASE;
    arg0_vaddr = load_u64(&stack[offset]);
    arg1_vaddr = load_u64(&stack[offset + sizeof(uint64_t)]);
    arg2_vaddr = load_u64(&stack[offset + 2U * sizeof(uint64_t)]);
    TEST_ASSERT_TRUE(stack_string_equals(stack, arg0_vaddr, "editor"));
    TEST_ASSERT_TRUE(stack_string_equals(stack, arg1_vaddr, "--safe"));
    TEST_ASSERT_TRUE(stack_string_equals(stack, arg2_vaddr, "file.txt"));
    TEST_ASSERT_EQUAL_UINT64(
        0, load_u64(&stack[offset + 3U * sizeof(uint64_t)]));
}

void test_panel_boot_argv_zero_argc_returns_null_argv(void) {
    uint8_t stack[TEST_STACK_SIZE] __attribute__((aligned(16)));
    uint64_t argv_vaddr = 0xfeedfaceULL;

    clear_stack(stack, sizeof(stack));
    TEST_ASSERT_EQUAL_UINT64(
        0, (uint64_t)panel_boot_place_argv_on_stack(
               stack, TEST_STACK_BASE, TEST_STACK_SIZE, 0, 0,
               &argv_vaddr));
    TEST_ASSERT_EQUAL_UINT64(0, argv_vaddr);
}

void test_panel_boot_argv_rejects_invalid_stack_inputs(void) {
    uint8_t stack[TEST_STACK_SIZE] __attribute__((aligned(16)));
    panel_boot_argv_t argv = {0};
    uint64_t argv_vaddr = 0xfeedfaceULL;

    argv.bytes[0] = 'x';
    argv.bytes[1] = '\0';
    argv.bytes_used = 2U;
    TEST_ASSERT_EQUAL_UINT64(
        (uint64_t)-1,
        (uint64_t)panel_boot_place_argv_on_stack(
            0, TEST_STACK_BASE, TEST_STACK_SIZE, &argv, 1U, &argv_vaddr));
    TEST_ASSERT_EQUAL_UINT64(
        (uint64_t)-1,
        (uint64_t)panel_boot_place_argv_on_stack(
            stack, TEST_STACK_BASE, 8U, &argv, 1U, &argv_vaddr));

    argv.offsets[0] = argv.bytes_used;
    TEST_ASSERT_EQUAL_UINT64(
        (uint64_t)-1,
        (uint64_t)panel_boot_place_argv_on_stack(
            stack, TEST_STACK_BASE, TEST_STACK_SIZE, &argv, 1U,
            &argv_vaddr));
}
