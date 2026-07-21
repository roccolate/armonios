#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel/irq.h"
#include "kernel/process.h"
#include "kernel/runtime_service.h"

static uint32_t g_eoi_seen;
static uint32_t g_backend_calls;
static uint32_t g_requeue_once;
static uint64_t g_counter_now;
static uint64_t g_counter_step;
static uint32_t g_metrics[RUNTIME_METRIC_COUNT];
static uint32_t g_queue_depth;
static uint32_t g_queue_high_water;
static uint64_t g_queue_overflow;

uint32_t board_irq_ack(void) { return 5U; }
void board_irq_end(uint32_t irq) { assert(irq == 5U); g_eoi_seen = 1U; }
int board_irq_is_spurious(uint32_t irq) { (void)irq; return 0; }
void uart_puts(const char *text) { (void)text; }
process_t *process_current(void) { return 0; }

void process_save_context(process_t *process, const uint64_t regs[31],
                          uint64_t pc, uint64_t pstate, uint64_t sp) {
    (void)process; (void)regs; (void)pc; (void)pstate; (void)sp;
}

int process_dispatch_next(process_t *current, exception_frame_t *frame,
                          process_dispatch_policy_t policy) {
    (void)current; (void)frame; (void)policy;
    return 0;
}

uint64_t runtime_service_counter_now(void) {
    uint64_t value = g_counter_now;
    g_counter_now += g_counter_step;
    return value;
}

void kernel_on_timer_tick(void) {
    assert(g_eoi_seen != 0U);
    g_backend_calls++;
    for (uint32_t i = 0; i < RUNTIME_METRIC_COUNT; i++) {
        runtime_service_report_metric(i, g_metrics[i]);
    }
    runtime_service_report_input_queue(g_queue_depth, g_queue_high_water,
                                       g_queue_overflow);
    if (g_requeue_once != 0U) {
        g_requeue_once = 0U;
        runtime_service_request(RUNTIME_WORK_PERIODIC);
    }
}

static runtime_service_stats_t snapshot(void) {
    runtime_service_stats_t stats;
    runtime_service_get_stats(&stats);
    return stats;
}

static void prepare(uint64_t step) {
    runtime_service_configure_timing(1000U, 10U);
    runtime_service_reset();
    g_eoi_seen = 1U;
    g_backend_calls = 0U;
    g_requeue_once = 0U;
    g_counter_now = 100U;
    g_counter_step = step;
    g_queue_depth = 0U;
    g_queue_high_water = 0U;
    g_queue_overflow = 0U;
    for (uint32_t i = 0; i < RUNTIME_METRIC_COUNT; i++) g_metrics[i] = 0U;
}

static void timing_and_coalescing(void) {
    runtime_service_stats_t stats;

    prepare(7U);
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    stats = snapshot();
    assert(g_backend_calls == 1U);
    assert(stats.request_count == 2U);
    assert(stats.coalesced_request_count == 1U);
    assert(stats.run_count == 1U);
    assert(stats.last_duration_ticks == 7U);
    assert(stats.max_duration_ticks == 7U);
    assert(stats.total_duration_ticks == 7U);
    assert(stats.over_budget_count == 0U);
    assert(stats.counter_frequency_hz == 1000U);
    assert(stats.budget_ticks == 10U);
    assert(stats.pending_work == 0U);

    assert(runtime_service_run_pending() == 0U);
    assert(snapshot().empty_run_count == 1U);
}

static void maximum_and_overrun(void) {
    runtime_service_stats_t stats;

    prepare(7U);
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    g_counter_step = 15U;
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    stats = snapshot();
    assert(stats.last_duration_ticks == 15U);
    assert(stats.max_duration_ticks == 15U);
    assert(stats.total_duration_ticks == 22U);
    assert(stats.over_budget_count == 1U);
}

static void work_metrics(void) {
    runtime_service_stats_t stats;

    prepare(5U);
    runtime_service_report_metric(RUNTIME_METRIC_INPUT_CONSUMED, 99U);
    runtime_service_report_input_queue(99U, 99U, 99U);

    g_metrics[RUNTIME_METRIC_INPUT_PRODUCED] = 5U;
    g_metrics[RUNTIME_METRIC_INPUT_CONSUMED] = 4U;
    g_metrics[RUNTIME_METRIC_REDRAW] = 1U;
    g_metrics[RUNTIME_METRIC_NETWORK_FRAMES] = 3U;
    g_metrics[RUNTIME_METRIC_DEVICE_POLLS] = 2U;
    g_queue_depth = 6U;
    g_queue_high_water = 7U;
    g_queue_overflow = 2U;
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);

    stats = snapshot();
    assert(stats.metric_last[RUNTIME_METRIC_INPUT_PRODUCED] == 5U);
    assert(stats.metric_last[RUNTIME_METRIC_INPUT_CONSUMED] == 4U);
    assert(stats.metric_last[RUNTIME_METRIC_REDRAW] == 1U);
    assert(stats.metric_last[RUNTIME_METRIC_NETWORK_FRAMES] == 3U);
    assert(stats.metric_last[RUNTIME_METRIC_DEVICE_POLLS] == 2U);
    assert(stats.metric_total[RUNTIME_METRIC_INPUT_PRODUCED] == 5U);
    assert(stats.metric_total[RUNTIME_METRIC_NETWORK_FRAMES] == 3U);
    assert(stats.metric_total[RUNTIME_METRIC_DEVICE_POLLS] == 2U);
    assert(stats.metric_max[RUNTIME_METRIC_INPUT_CONSUMED] == 4U);
    assert(stats.metric_max[RUNTIME_METRIC_NETWORK_FRAMES] == 3U);
    assert(stats.metric_max[RUNTIME_METRIC_DEVICE_POLLS] == 2U);
    assert(stats.max_input_queue_depth == 6U);
    assert(stats.input_queue_high_water == 7U);
    assert(stats.input_queue_overflow_count == 2U);

    g_metrics[RUNTIME_METRIC_INPUT_PRODUCED] = 2U;
    g_metrics[RUNTIME_METRIC_INPUT_CONSUMED] = 6U;
    g_metrics[RUNTIME_METRIC_REDRAW] = 0U;
    g_metrics[RUNTIME_METRIC_NETWORK_FRAMES] = 7U;
    g_metrics[RUNTIME_METRIC_DEVICE_POLLS] = 5U;
    g_queue_depth = 3U;
    g_queue_overflow = 4U;
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);

    stats = snapshot();
    assert(stats.metric_last[RUNTIME_METRIC_INPUT_PRODUCED] == 2U);
    assert(stats.metric_total[RUNTIME_METRIC_INPUT_PRODUCED] == 7U);
    assert(stats.metric_max[RUNTIME_METRIC_INPUT_PRODUCED] == 5U);
    assert(stats.metric_last[RUNTIME_METRIC_INPUT_CONSUMED] == 6U);
    assert(stats.metric_total[RUNTIME_METRIC_INPUT_CONSUMED] == 10U);
    assert(stats.metric_max[RUNTIME_METRIC_INPUT_CONSUMED] == 6U);
    assert(stats.metric_last[RUNTIME_METRIC_REDRAW] == 0U);
    assert(stats.metric_total[RUNTIME_METRIC_REDRAW] == 1U);
    assert(stats.metric_last[RUNTIME_METRIC_NETWORK_FRAMES] == 7U);
    assert(stats.metric_total[RUNTIME_METRIC_NETWORK_FRAMES] == 10U);
    assert(stats.metric_max[RUNTIME_METRIC_NETWORK_FRAMES] == 7U);
    assert(stats.metric_last[RUNTIME_METRIC_DEVICE_POLLS] == 5U);
    assert(stats.metric_total[RUNTIME_METRIC_DEVICE_POLLS] == 7U);
    assert(stats.metric_max[RUNTIME_METRIC_DEVICE_POLLS] == 5U);
    assert(stats.max_input_queue_depth == 6U);
    assert(stats.input_queue_overflow_count == 4U);
}

static void requeue_and_reset(void) {
    runtime_service_stats_t stats;

    prepare(4U);
    g_requeue_once = 1U;
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    stats = snapshot();
    assert(stats.requeue_count == 1U);
    assert(stats.pending_work == RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);

    runtime_service_reset();
    runtime_service_get_stats(0);
    runtime_service_report_metric(RUNTIME_METRIC_COUNT, 1U);
    stats = snapshot();
    assert(stats.request_count == 0U);
    assert(stats.metric_total[RUNTIME_METRIC_INPUT_CONSUMED] == 0U);
    assert(stats.metric_total[RUNTIME_METRIC_NETWORK_FRAMES] == 0U);
    assert(stats.metric_total[RUNTIME_METRIC_DEVICE_POLLS] == 0U);
    assert(stats.counter_frequency_hz == 1000U);
    assert(stats.budget_ticks == 10U);
}

static void eoi_order(void) {
    prepare(3U);
    g_eoi_seen = 0U;
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    irq_handler();
    assert(g_eoi_seen == 1U);
    assert(g_backend_calls == 1U);
    assert(snapshot().last_duration_ticks == 3U);
}

int main(void) {
    timing_and_coalescing();
    maximum_and_overrun();
    work_metrics();
    requeue_and_reset();
    eoi_order();
    puts("deferred runtime service device/network telemetry: ok");
    return 0;
}
