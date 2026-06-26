#include "kernel/init_status.h"

static const char g_init_status_names[] =
    "board\0"
    "dtb\0"
    "pmm\0"
    "kheap\0"
    "vmm\0"
    "console\0"
    "vfs\0"
    "irq/timer\0"
    "storage\0"
    "display\0"
    "network\0"
    "input\0"
    "panel\0"
    "sched";

static const uint8_t g_init_status_name_offsets[INIT_PHASE_COUNT] = {
    [INIT_PHASE_BOARD] = 0,
    [INIT_PHASE_DTB] = 6,
    [INIT_PHASE_PMM] = 10,
    [INIT_PHASE_KHEAP] = 14,
    [INIT_PHASE_VMM] = 20,
    [INIT_PHASE_CONSOLE] = 24,
    [INIT_PHASE_VFS] = 32,
    [INIT_PHASE_IRQ_TIMER] = 36,
    [INIT_PHASE_STORAGE] = 46,
    [INIT_PHASE_DISPLAY] = 54,
    [INIT_PHASE_NETWORK] = 62,
    [INIT_PHASE_INPUT] = 70,
    [INIT_PHASE_PANEL] = 76,
    [INIT_PHASE_SCHED] = 82,
};

static uint8_t g_init_status_values[INIT_PHASE_COUNT];
static init_status_entry_t g_init_status_entry;

void init_status_reset(void) {
    for (uint32_t i = 0; i < INIT_PHASE_COUNT; i++) {
        g_init_status_values[i] = INIT_STATUS_SKIPPED;
    }
}

void init_status_set(init_phase_t phase, init_status_t status) {
    if ((uint32_t)phase >= INIT_PHASE_COUNT) {
        return;
    }

    g_init_status_values[phase] = status;
}

init_status_t init_status_get(init_phase_t phase) {
    if ((uint32_t)phase >= INIT_PHASE_COUNT) {
        return INIT_STATUS_FAIL;
    }

    return (init_status_t)g_init_status_values[phase];
}

const init_status_entry_t *init_status_at(uint32_t index) {
    if (index >= INIT_PHASE_COUNT) {
        return 0;
    }

    g_init_status_entry.name =
        &g_init_status_names[g_init_status_name_offsets[index]];
    g_init_status_entry.status = (init_status_t)g_init_status_values[index];
    return &g_init_status_entry;
}

uint32_t init_status_count(void) {
    return INIT_PHASE_COUNT;
}

const char *init_status_label(init_status_t status) {
    switch (status) {
    case INIT_STATUS_OK:
        return "OK";
    case INIT_STATUS_WARN:
        return "WARN";
    case INIT_STATUS_FAIL:
        return "FAIL";
    case INIT_STATUS_SKIPPED:
    default:
        return "SKIPPED";
    }
}
