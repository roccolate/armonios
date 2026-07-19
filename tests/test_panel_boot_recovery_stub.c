/*
 * Host-side stubs for the panel_boot recovery unit tests.
 *
 * The real panel_boot_run in kernel/panel_boot.c sets up a page
 * table, switches TTBR0_EL1, and enters EL0, all of which require
 * the AArch64 toolchain and the real kernel memory map. The
 * recovery unit tests only need to exercise the
 * panel_boot_recovery_decide policy and verify that
 * panel_boot_recovery_run loops up to PANEL_BOOT_RECOVERY_MAX_ATTEMPTS
 * before stopping; they never need a real panel run.
 *
 * We provide counter-based run/log callbacks so tests can detect
 * that the wrapper invoked them without depending on UART.
 */

#include <stdint.h>

static uint64_t g_stub_call_count;
static uint64_t g_stub_log_count;
static uint64_t g_stub_next_exit = 0xCAFEBABEULL;

void test_panel_boot_recovery_reset_stub(uint64_t next_exit) {
    g_stub_call_count = 0;
    g_stub_log_count = 0;
    g_stub_next_exit = next_exit;
}

uint32_t test_panel_boot_recovery_call_count(void) {
    return (uint32_t)g_stub_call_count;
}

uint32_t test_panel_boot_recovery_log_count(void) {
    return (uint32_t)g_stub_log_count;
}

uint64_t test_panel_boot_recovery_run_stub(void *ctx) {
    (void)ctx;

    g_stub_call_count++;
    return g_stub_next_exit;
}

void test_panel_boot_recovery_log_stub(const char *line) {
    if (line != 0) {
        g_stub_log_count++;
    }
}
