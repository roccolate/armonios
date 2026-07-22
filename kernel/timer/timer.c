#include "kernel/timer/timer.h"

#include <stdint.h>

#include "kernel/runtime_service.h"
#include "kernel/sched/sched.h"

/*
 * AArch64 physical timer driver.
 *
 * timer_init programs CNTP_CVAL/CTL using CNTFRQ_EL0. The hard-IRQ handler is
 * intentionally bounded: account the tick, rearm the timer, publish coalescible
 * periodic, input, and network work, and advance scheduler counters. Device,
 * GUI, and network work runs later through the post-EOI runtime service.
 */

static uint64_t g_ticks;
static uint64_t g_interval_ticks;
static uint64_t g_next_cval;

static uint64_t read_cntfrq(void) {
    uint64_t value;

    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
}

static uint64_t read_cntpct(void) {
    uint64_t value;

    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(value));
    return value;
}

uint64_t runtime_service_counter_now(void) {
    return read_cntpct();
}

static void write_cntp_cval(uint64_t value) {
    __asm__ volatile("msr cntp_cval_el0, %0" :: "r"(value));
}

static void write_cntp_ctl(uint64_t value) {
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(value));
}

void timer_init(uint32_t hz) {
    uint64_t freq = read_cntfrq();

    if (hz == 0) {
        hz = 1;
    }

    g_interval_ticks = freq / hz;
    if (g_interval_ticks == 0) {
        g_interval_ticks = 1;
    }

    /* One timer interval is the initial observation threshold. Exceeding it
     * means a service pass delayed EL0 beyond the next nominal timer deadline;
     * later budget work will introduce smaller per-subsystem limits. */
    runtime_service_configure_timing(freq, g_interval_ticks);

    g_next_cval = read_cntpct() + g_interval_ticks;
    write_cntp_cval(g_next_cval);
    write_cntp_ctl(1);
}

void timer_handle_irq(void *context) {
    (void)context;
    g_ticks++;

    /* Advance CVAL by the fixed interval so ticks are anchored to the
     * previous expiry, not the moment we service the IRQ. This eliminates
     * cumulative drift that TVAL reloads cause. */
    g_next_cval += g_interval_ticks;
    write_cntp_cval(g_next_cval);

    runtime_service_request(RUNTIME_WORK_PERIODIC | RUNTIME_WORK_INPUT |
                            RUNTIME_WORK_NETWORK);
    sched_on_timer_tick();
}

uint64_t timer_ticks(void) {
    return g_ticks;
}
