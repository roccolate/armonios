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
static runtime_service_work_report_t g_backend_report;

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

uint64_t runtime_service_counter_now(void) {
    uint64_t value = g_counter_now;

    g_counter_now += g_counter_step;
    return value;
}

void kernel_on_timer_tick(void) {
    assert(g_eoi_seen != 0U);
    assert(runtime_service_is_active());
    g_backend_calls++;
    runtime_service_report_work(&g_backend_report);
    if (g_requeue_once != 0U) {
        g_requeue_once = 0U;
        runtime_service_request(RUNTIME_WORK_PERIODIC);
    }
}

static runtime_service_stats_t stats_snapshot(void) {
    runtime_service_stats_t stats;

    runtime_service_get_stats(&stats);
    return stats;
}

static void prepare_service(uint64_t counter_step) {
    runtime_service_configure_timing(1000U, 10U);
    runtime_service_reset();
    g_eoi_seen = 1U;
    g_backend_calls = 0U;
    g_requeue_once = 0U;
    g_counter_now = 100U;
    g_counter_step = counter_step;
    g_backend_report = (runtime_service_work_report_t){0};
}

static void repeated_ticks_coalesce_and_are_measured(void) {
    runtime_service_stats_t stats;

    prepare_service(7U);

    runtime_service_request(RUNTIME_WORK_PERIODIC);
    runtime_service_request(RUNTIME_WORK_PERIODIC);

    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    assert(g_backend_calls == 1U);
    assert(!runtime_service_is_active());

    stats = stats_snapshot();
    assert(stats.request_count == 2U);
    assert(stats.coalesced_request_count == 1U);
    assert(stats.run_count == 1U);
    assert(stats.empty_run_count == 0U);
    assert(stats.requeue_count == 0U);
    assert(stats.last_duration_ticks == 7U);
    assert(stats.max_duration_ticks == 7U);
    assert(stats.total_duration_ticks == 7U);
    assert(stats.over_budget_count == 0U);
    assert(stats.counter_frequency_hz == 1000U);
    assert(stats.budget_ticks == 10U);
    assert(stats.pending_work == 0U);
    assert(stats.last_work == RUNTIME_WORK_PERIODIC);

    assert(runtime_service_run_pending() == 0U);
    stats = stats_snapshot();
    assert(stats.empty_run_count == 1U);
    assert(stats.run_count == 1U);
    assert(stats.last_work == 0U);
    assert(stats.last_input_events_consumed == 0U);
}

static void maximum_and_over_budget_counts_track(void) {
    runtime_service_stats_t stats;

    prepare_service(7U);
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);

    g_counter_step = 15U;
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);

    stats = stats_snapshot();
    assert(stats.request_count == 2U);
    assert(stats.run_count == 2U);
    assert(stats.last_duration_ticks == 15U);
    assert(stats.max_duration_ticks == 15U);
    assert(stats.total_duration_ticks == 22U);
    assert(stats.over_budget_count == 1U);
}

static void per_class_work_metrics_track_last_max_and_total(void) {
    runtime_service_stats_t stats;
    const runtime_service_work_report_t ignored = {
        .input_events_consumed = 99U,
        .input_queue_overflow_count = 99U,
    };

    prepare_service(5U);
    runtime_service_report_work(&ignored);

    g_backend_report = (runtime_service_work_report_t){
        .board_input_events = 2U,
        .usb_input_events = 3U,
        .input_events_consumed = 4U,
        .input_queue_depth_after_producers = 5U,
        .input_queue_high_water = 6U,
        .input_queue_overflow_count = 7U,
        .redraw_count = 1U,
        .network_frames = 8U,
    };
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);

    stats = stats_snapshot();
    assert(stats.last_board_input_events == 2U);
    assert(stats.max_board_input_events == 2U);
    assert(stats.total_board_input_events == 2U);
    assert(stats.last_usb_input_events == 3U);
    assert(stats.max_usb_input_events == 3U);
    assert(stats.total_usb_input_events == 3U);
    assert(stats.last_input_events_consumed == 4U);
    assert(stats.max_input_events_consumed == 4U);
    assert(stats.total_input_events_consumed == 4U);
    assert(stats.max_input_queue_depth == 5U);
    assert(stats.input_queue_high_water == 6U);
    assert(stats.input_queue_overflow_count == 7U);
    assert(stats.last_redraw_count == 1U);
    assert(stats.max_redraw_count == 1U);
    assert(stats.total_redraw_count == 1U);
    assert(stats.last_network_frames == 8U);
    assert(stats.max_network_frames == 8U);
    assert(stats.total_network_frames == 8U);

    g_backend_report = (runtime_service_work_report_t){
        .board_input_events = 1U,
        .usb_input_events = 4U,
        .input_events_consumed = 2U,
        .input_queue_depth_after_producers = 3U,
        .input_queue_high_water = 6U,
        .input_queue_overflow_count = 9U,
        .network_frames = 1U,
    };
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);

    stats = stats_snapshot();
    assert(stats.last_board_input_events == 1U);
    assert(stats.max_board_input_events == 2U);
    assert(stats.total_board_input_events == 3U);
    assert(stats.last_usb_input_events == 4U);
    assert(stats.max_usb_input_events == 4U);
    assert(stats.total_usb_input_events == 7U);
    assert(stats.last_input_events_consumed == 2U);
    assert(stats.max_input_events_consumed == 4U);
    assert(stats.total_input_events_consumed == 6U);
    assert(stats.max_input_queue_depth == 5U);
    assert(stats.input_queue_high_water == 6U);
    assert(stats.input_queue_overflow_count == 9U);
    assert(stats.last_redraw_count == 0U);
    assert(stats.max_redraw_count == 1U);
    assert(stats.total_redraw_count == 1U);
    assert(stats.last_network_frames == 1U);
    assert(stats.max_network_frames == 8U);
    assert(stats.total_network_frames == 9U);
}

static void backend_requeue_survives_and_is_counted(void) {
    runtime_service_stats_t stats;

    prepare_service(4U);
    g_requeue_once = 1U;

    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    assert(g_backend_calls == 1U);

    stats = stats_snapshot();
    assert(stats.request_count == 2U);
    assert(stats.coalesced_request_count == 0U);
    assert(stats.run_count == 1U);
    assert(stats.requeue_count == 1U);
    assert(stats.pending_work == RUNTIME_WORK_PERIODIC);

    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    assert(g_backend_calls == 2U);

    stats = stats_snapshot();
    assert(stats.run_count == 2U);
    assert(stats.requeue_count == 1U);
    assert(stats.total_duration_ticks == 8U);
    assert(stats.pending_work == 0U);
}

static void reset_clears_telemetry_but_preserves_timing(void) {
    runtime_service_stats_t stats;

    prepare_service(12U);
    g_backend_report.input_events_consumed = 3U;
    g_backend_report.input_queue_high_water = 4U;
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);

    runtime_service_reset();
    runtime_service_get_stats(0);
    runtime_service_report_work(0);
    stats = stats_snapshot();

    assert(stats.request_count == 0U);
    assert(stats.coalesced_request_count == 0U);
    assert(stats.run_count == 0U);
    assert(stats.empty_run_count == 0U);
    assert(stats.requeue_count == 0U);
    assert(stats.last_duration_ticks == 0U);
    assert(stats.max_duration_ticks == 0U);
    assert(stats.total_duration_ticks == 0U);
    assert(stats.over_budget_count == 0U);
    assert(stats.counter_frequency_hz == 1000U);
    assert(stats.budget_ticks == 10U);
    assert(stats.total_input_events_consumed == 0U);
    assert(stats.input_queue_high_water == 0U);
    assert(stats.pending_work == 0U);
    assert(stats.last_work == 0U);
    assert(!runtime_service_is_active());
}

static void irq_closes_before_deferred_work(void) {
    prepare_service(3U);
    g_eoi_seen = 0U;

    runtime_service_request(RUNTIME_WORK_PERIODIC);
    irq_handler();

    assert(g_eoi_seen == 1U);
    assert(g_backend_calls == 1U);
    assert(stats_snapshot().last_duration_ticks == 3U);
}

int main(void) {
    repeated_ticks_coalesce_and_are_measured();
    maximum_and_over_budget_counts_track();
    per_class_work_metrics_track_last_max_and_total();
    backend_requeue_survives_and_is_counted();
    reset_clears_telemetry_but_preserves_timing();
    irq_closes_before_deferred_work();
    puts("deferred runtime service work telemetry: ok");
    return 0;
}
