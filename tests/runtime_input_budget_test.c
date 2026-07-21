#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "input/input.h"
#include "kernel/runtime_service.h"

static uint32_t g_fake_input_events;
static uint32_t g_input_poll_calls;
static uint32_t g_input_events_returned;
static uint32_t g_backend_calls;
static uint64_t g_counter_now;

uint64_t runtime_service_counter_now(void) {
    return g_counter_now++;
}

int input_queue_poll(input_event_t *event) {
    g_input_poll_calls++;
    if (event == 0 || g_fake_input_events == 0U) {
        return -1;
    }

    g_fake_input_events--;
    g_input_events_returned++;
    event->type = INPUT_EVENT_KEY_PRESS;
    event->timestamp = 0U;
    event->data.key.key = 'a';
    runtime_service_report_metric(RUNTIME_METRIC_INPUT_CONSUMED, 1U);
    runtime_service_report_input_queue(g_fake_input_events + 1U,
                                       g_fake_input_events + 1U, 0U);
    return 0;
}

int input_queue_available(void) {
    return (int)g_fake_input_events;
}

void kernel_on_timer_tick(void) {
    input_event_t event;

    g_backend_calls++;
    while (runtime_service_input_poll(&event) == 0) {
    }
}

static runtime_service_stats_t snapshot(void) {
    runtime_service_stats_t stats;

    runtime_service_get_stats(&stats);
    return stats;
}

static void prepare(uint32_t events) {
    runtime_service_configure_timing(1000U, 10U);
    runtime_service_reset();
    g_fake_input_events = events;
    g_input_poll_calls = 0U;
    g_input_events_returned = 0U;
    g_backend_calls = 0U;
    g_counter_now = 100U;
}

static void exact_budget_finishes_without_requeue(void) {
    runtime_service_stats_t stats;

    prepare(RUNTIME_INPUT_EVENT_BUDGET);
    runtime_service_request(RUNTIME_WORK_INPUT);
    assert(runtime_service_run_pending() == RUNTIME_WORK_INPUT);
    stats = snapshot();

    assert(g_backend_calls == 1U);
    assert(g_input_events_returned == RUNTIME_INPUT_EVENT_BUDGET);
    assert(g_fake_input_events == 0U);
    assert(stats.metric_last[RUNTIME_METRIC_INPUT_CONSUMED] ==
           RUNTIME_INPUT_EVENT_BUDGET);
    assert(stats.input_budget_exhaustion_count == 0U);
    assert(stats.pending_work == 0U);
    assert(stats.requeue_count == 0U);
}

static void leftover_event_is_requeued_and_completed(void) {
    runtime_service_stats_t stats;

    prepare(RUNTIME_INPUT_EVENT_BUDGET + 1U);
    runtime_service_request(RUNTIME_WORK_INPUT);
    assert(runtime_service_run_pending() == RUNTIME_WORK_INPUT);
    stats = snapshot();

    assert(g_backend_calls == 1U);
    assert(g_input_events_returned == RUNTIME_INPUT_EVENT_BUDGET);
    assert(g_fake_input_events == 1U);
    assert(stats.metric_last[RUNTIME_METRIC_INPUT_CONSUMED] ==
           RUNTIME_INPUT_EVENT_BUDGET);
    assert(stats.input_budget_exhaustion_count == 1U);
    assert(stats.pending_work == RUNTIME_WORK_INPUT);
    assert(stats.requeue_count == 1U);

    assert(runtime_service_run_pending() == RUNTIME_WORK_INPUT);
    stats = snapshot();
    assert(g_backend_calls == 2U);
    assert(g_input_events_returned == RUNTIME_INPUT_EVENT_BUDGET + 1U);
    assert(g_fake_input_events == 0U);
    assert(stats.metric_last[RUNTIME_METRIC_INPUT_CONSUMED] == 1U);
    assert(stats.metric_total[RUNTIME_METRIC_INPUT_CONSUMED] ==
           RUNTIME_INPUT_EVENT_BUDGET + 1U);
    assert(stats.input_budget_exhaustion_count == 1U);
    assert(stats.pending_work == 0U);
}

static void periodic_and_input_share_one_backend_call(void) {
    runtime_service_stats_t stats;

    prepare(3U);
    runtime_service_request(RUNTIME_WORK_PERIODIC | RUNTIME_WORK_INPUT);
    assert(runtime_service_run_pending() ==
           (RUNTIME_WORK_PERIODIC | RUNTIME_WORK_INPUT));
    stats = snapshot();

    assert(g_backend_calls == 1U);
    assert(g_input_events_returned == 3U);
    assert(stats.metric_last[RUNTIME_METRIC_INPUT_CONSUMED] == 3U);
    assert(stats.pending_work == 0U);
}

static void outside_service_is_not_budgeted(void) {
    runtime_service_stats_t stats;
    input_event_t event;

    prepare(RUNTIME_INPUT_EVENT_BUDGET + 1U);
    while (runtime_service_input_poll(&event) == 0) {
    }
    stats = snapshot();

    assert(g_input_events_returned == RUNTIME_INPUT_EVENT_BUDGET + 1U);
    assert(g_fake_input_events == 0U);
    assert(stats.metric_total[RUNTIME_METRIC_INPUT_CONSUMED] == 0U);
    assert(stats.input_budget_exhaustion_count == 0U);
    assert(stats.pending_work == 0U);
}

static void reset_clears_input_exhaustion(void) {
    runtime_service_stats_t stats;

    prepare(RUNTIME_INPUT_EVENT_BUDGET + 1U);
    runtime_service_request(RUNTIME_WORK_INPUT);
    (void)runtime_service_run_pending();
    assert(snapshot().input_budget_exhaustion_count == 1U);

    runtime_service_reset();
    stats = snapshot();
    assert(stats.input_budget_exhaustion_count == 0U);
    assert(stats.pending_work == 0U);
    assert(stats.counter_frequency_hz == 1000U);
    assert(stats.budget_ticks == 10U);
}

int main(void) {
    exact_budget_finishes_without_requeue();
    leftover_event_is_requeued_and_completed();
    periodic_and_input_share_one_backend_call();
    outside_service_is_not_budgeted();
    reset_clears_input_exhaustion();
    puts("deferred runtime service input budget: ok");
    return 0;
}
