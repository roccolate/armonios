#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel/gui.h"
#include "kernel/process.h"

static uint32_t g_closed_pid;
static uint32_t g_close_calls;
static uint32_t g_gui_pid;
static uint32_t g_gui_calls;

uint32_t vfs_close_all_for_pid(uint32_t pid) {
    g_closed_pid = pid;
    g_close_calls++;
    return 2U;
}

gui_desktop_t *gui_desktop(void) {
    return 0;
}

void gui_destroy_windows_for_pid(gui_desktop_t *desktop, uint32_t pid) {
    (void)desktop;
    g_gui_pid = pid;
    g_gui_calls++;
}

void pmm_free_page(uint64_t paddr) {
    (void)paddr;
}

void vmm_free_table(uint64_t *pgd) {
    (void)pgd;
}

static void reset_calls(void) {
    g_closed_pid = 0;
    g_close_calls = 0;
    g_gui_pid = 0;
    g_gui_calls = 0;
}

static void verify_exit_cleanup_once(void) {
    process_t process;

    reset_calls();
    process_init(&process, 77U, "cleanup");
    process_mark_exited(&process, 0x55U);

    assert(process.state == PROCESS_ZOMBIE);
    assert(process.exit_code == 0x55U);
    assert(g_close_calls == 1U);
    assert(g_closed_pid == 77U);
    assert(g_gui_calls == 1U);
    assert(g_gui_pid == 77U);

    process_mark_exited(&process, 0x99U);
    assert(process.exit_code == 0x55U);
    assert(g_close_calls == 1U);
    assert(g_gui_calls == 1U);
}

static void verify_kill_uses_central_cleanup(void) {
    process_t *process;

    reset_calls();
    process_table_init();
    process = process_alloc(88U, "kill-cleanup");
    assert(process != 0);
    assert(process_kill(88U, 0x80U) == 0);
    assert(process->state == PROCESS_ZOMBIE);
    assert(g_close_calls == 1U);
    assert(g_closed_pid == 88U);
}

int main(void) {
    verify_exit_cleanup_once();
    verify_kill_uses_central_cleanup();
    puts("PASS: process exit closes VFS descriptors");
    return 0;
}
