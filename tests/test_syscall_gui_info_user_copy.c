#include <stdint.h>
#include <stdlib.h>

#include "fb/fb.h"
#include "kernel/gui.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "kernel/process.h"
#include "kernel/sched/sched.h"
#include "kernel/syscall_helpers.h"
#include "kernel/syscall_internal.h"

#define TEST_PMM_PAGES 64U
#define TEST_FB_W 64U
#define TEST_FB_H 48U

typedef struct {
    uint32_t pid;
    uint32_t state;
    char name[16];
} test_proc_entry_t;

static uint32_t g_output_copy_pixels[TEST_FB_W * TEST_FB_H];

static void output_check(int ok) {
    if (!ok) {
        __builtin_trap();
    }
}

#define CHECK_TRUE(expr) output_check((expr) != 0)
#define CHECK_EQ(expected, actual) output_check((expected) == (actual))

uint64_t timer_ticks(void) {
    return 77U;
}

static uint32_t load_u32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) |
           ((uint32_t)p[3] << 24U);
}

static uint64_t load_u64(const uint8_t *p) {
    uint64_t value = 0U;

    for (uint32_t i = 0; i < sizeof(uint64_t); i++) {
        value |= (uint64_t)p[i] << (i * 8U);
    }
    return value;
}

static int text_equals(const char *actual, const char *expected) {
    uint32_t i = 0U;

    for (;;) {
        if (actual[i] != expected[i]) {
            return 0;
        }
        if (actual[i] == '\0') {
            return 1;
        }
        i++;
    }
}

static void clear_page(uint8_t *page) {
    for (uint64_t i = 0; i < PAGE_SIZE; i++) {
        page[i] = 0U;
    }
}

static void test_gui_info_output_copies(void) {
    void *pmm_memory = 0;
    uint8_t *user_rw = 0;
    uint8_t *user_ro = 0;
    uint64_t *pgd;
    process_t *process;
    gui_desktop_t *desktop;
    gui_window_t *window;
    fb_t fb;
    uint64_t rw_base;
    uint64_t ro_base;
    uint32_t window_id;
    uint32_t queued;
    int64_t result;

    CHECK_EQ(0, posix_memalign(&pmm_memory, PAGE_SIZE,
                               TEST_PMM_PAGES * PAGE_SIZE));
    pmm_init((uint64_t)(uintptr_t)pmm_memory, TEST_PMM_PAGES * PAGE_SIZE);
    pgd = vmm_new_table();
    CHECK_TRUE(pgd != 0);

    CHECK_EQ(0, posix_memalign((void **)&user_rw, PAGE_SIZE, PAGE_SIZE));
    CHECK_EQ(0, posix_memalign((void **)&user_ro, PAGE_SIZE, PAGE_SIZE));
    clear_page(user_rw);
    clear_page(user_ro);

    rw_base = (uint64_t)(uintptr_t)user_rw;
    ro_base = (uint64_t)(uintptr_t)user_ro;
    process_table_init();
    process = process_alloc(401U, "copy-out");
    CHECK_TRUE(process != 0);
    process_set_page_table(process, pgd);
    CHECK_EQ(0, process_add_user_region(process, rw_base, PAGE_SIZE));
    CHECK_EQ(0, process_add_user_region(process, ro_base, PAGE_SIZE));
    CHECK_EQ(0, vmm_map_page(pgd, rw_base, rw_base,
                             VMM_FLAG_READ | VMM_FLAG_WRITE |
                                 VMM_FLAG_USER));
    CHECK_EQ(0, vmm_map_page(pgd, ro_base, ro_base,
                             VMM_FLAG_READ | VMM_FLAG_USER));

    CHECK_EQ(0, fb_init(&fb, g_output_copy_pixels,
                        TEST_FB_W, TEST_FB_H, TEST_FB_W));
    gui_init_for_framebuffer(&fb, 0U);
    desktop = gui_desktop();
    CHECK_TRUE(desktop != 0);
    CHECK_EQ(0, gui_create_window_for_pid(desktop, process->pid,
                                          3U, 4U, 20U, 18U,
                                          0U, 0U, "copy", &window_id));
    window = &desktop->windows[window_id];

    CHECK_EQ(ERR_PERM, sys_window_get_bounds(process, window_id, ro_base));
    CHECK_EQ(0, sys_window_get_bounds(process, window_id, rw_base));
    CHECK_EQ(3U, load_u32(&user_rw[0]));
    CHECK_EQ(4U, load_u32(&user_rw[4]));
    CHECK_EQ(20U, load_u32(&user_rw[8]));
    CHECK_EQ(18U, load_u32(&user_rw[12]));

    window->minimized = 1U;
    desktop->focused_window_id = window_id;
    CHECK_EQ(ERR_PERM, sys_window_state(process, window_id, ro_base));
    CHECK_EQ(0, sys_window_state(process, window_id, rw_base + 32U));
    CHECK_EQ(GUI_WINDOW_STATE_MINIMIZED | GUI_WINDOW_STATE_FOCUSED,
             load_u32(&user_rw[32]));

    CHECK_EQ(0, gui_window_push_event(window, GUI_EVENT_RESIZE, 33, 44));
    queued = window->event_count;
    CHECK_EQ(ERR_PERM,
             sys_window_event(process, window_id, ro_base, 1U));
    CHECK_EQ(queued, window->event_count);
    result = sys_window_event(process, window_id, rw_base + 64U, 1U);
    CHECK_EQ(1, result);
    CHECK_EQ(GUI_EVENT_RESIZE, load_u32(&user_rw[64]));
    CHECK_EQ(33U, load_u32(&user_rw[68]));
    CHECK_EQ(44U, load_u32(&user_rw[72]));
    CHECK_EQ(0U, window->event_count);

    CHECK_EQ(ERR_PERM, sys_meminfo(process, ro_base));
    CHECK_EQ(0, sys_meminfo(process, rw_base + 128U));
    CHECK_EQ(pmm_total_count(), load_u64(&user_rw[128]));
    CHECK_EQ(pmm_free_count(), load_u64(&user_rw[136]));

    CHECK_EQ(ERR_PERM, sys_timeinfo(process, ro_base));
    CHECK_EQ(0, sys_timeinfo(process, rw_base + 160U));
    CHECK_EQ(77U, load_u64(&user_rw[160]));
    CHECK_EQ(sched_ticks(), load_u64(&user_rw[168]));
    CHECK_EQ(sched_quantums(), load_u64(&user_rw[176]));

    CHECK_EQ(ERR_PERM, sys_proclist(process, ro_base, 1U));
    result = sys_proclist(process, rw_base + 256U, 1U);
    CHECK_EQ(1, result);
    CHECK_EQ(process->pid, load_u32(&user_rw[256]));
    CHECK_EQ((uint32_t)process->state, load_u32(&user_rw[260]));
    CHECK_TRUE(text_equals((const char *)&user_rw[264], "copy-out"));

    gui_destroy_windows_for_pid(desktop, process->pid);
    process_table_init();
    vmm_free_table(pgd);
    free(user_ro);
    free(user_rw);
    free(pmm_memory);
}

__attribute__((constructor))
static void test_syscall_gui_info_user_copy_constructor(void) {
    test_gui_info_output_copies();
}
