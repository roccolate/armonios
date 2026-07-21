#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel/process.h"
#include "kernel/runtime_service.h"

static uint32_t g_eoi_seen;
static uint32_t g_backend_calls;
static uint32_t g_requeue_once;

uint32_t board_irq_ack(void) {
    return 5U;
}

void board_irq_end(uint32_t irq) {
    assert(irq == 5U);
    g_eoi_seen = 1U;
}

int board_irq_is_spurious(uint32_t irq) {
    (void)irq;
    return 0;
}

void uart_puts(const char *text) {
    (void)text;
}

process_t *process_current(void) {
    return 0;
}

void process_save_context(process_t *process, const uint64_t regs[31],
                          uint64_t pc, uint64_t pstate, uint64_t sp) {
    (void)process;
    (void)regs;
    (void)pc;
    (void)pstate;
    (void)sp;
}

int process_dispatch_next(process_t *current, exception_frame_t *frame,
                          process_dispatch_policy_t policy) {
    (void)current;
    (void)frame;
    (void)policy;
    return 0;
}

void kernel_on_timer_tick(void) {
    assert(g_eoi_seen != 0U);
    g_backend_calls++;
    if (g_requeue_once != 0U) {
        g_requeue_once = 0U;
        runtime_service_request(RUNTIME_WORK_PERIODIC);
    }
}

static void repeated_ticks_coalesce(void) {
    runtime_service_reset();
    g_backend_calls = 0U;

    runtime_service_request(RUNTIME_WORK_PERIODIC);
    runtime_service_request(RUNTIME_WORK_PERIODIC);

    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    assert(g_backend_calls == 1U);
    assert(runtime_service_run_pending() == 0U);
}

static void backend_requeue_survives_current_pass(void) {
    runtime_service_reset();
    g_backend_calls = 0U;
    g_requeue_once = 1U;

    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    assert(g_backend_calls == 1U);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    assert(g_backend_calls == 2U);
}

static void irq_closes_before_deferred_work(void) {
    runtime_service_reset();
    g_eoi_seen = 0U;
    g_backend_calls = 0U;

    runtime_service_request(RUNTIME_WORK_PERIODIC);
    irq_handler();

    assert(g_eoi_seen == 1U);
    assert(g_backend_calls == 1U);
}

int main(void) {
    repeated_ticks_coalesce();
    backend_requeue_survives_current_pass();
    irq_closes_before_deferred_work();
    puts("deferred runtime service: ok");
    return 0;
}
