#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "input/input.h"
#include "kernel/runtime_service.h"

void irq_disable(void) {
}

void irq_enable(void) {
}

void uart_pump_input(void) {
}

int runtime_service_is_active(void) {
    return 0;
}

void runtime_service_report_work(const runtime_service_work_report_t *report) {
    (void)report;
}

static input_event_t key_event(uint32_t key) {
    input_event_t event = {
        .type = INPUT_EVENT_KEY_PRESS,
        .timestamp = 0,
        .data.key.key = key,
    };
    return event;
}

int main(void) {
    input_queue_stats_t stats;
    input_event_t event;

    input_queue_init();
    input_queue_get_stats(&stats);
    assert(stats.accepted_push_count == 0U);
    assert(stats.overflow_count == 0U);
    assert(stats.current_depth == 0U);
    assert(stats.high_water == 0U);

    for (uint32_t i = 0; i < INPUT_EVENT_QUEUE_SIZE; i++) {
        event = key_event(i);
        assert(input_queue_push(&event) == 0);
    }

    input_queue_get_stats(&stats);
    assert(stats.accepted_push_count == INPUT_EVENT_QUEUE_SIZE);
    assert(stats.overflow_count == 0U);
    assert(stats.current_depth == INPUT_EVENT_QUEUE_SIZE);
    assert(stats.high_water == INPUT_EVENT_QUEUE_SIZE);

    event = key_event(0x55U);
    assert(input_queue_push(&event) == -1);
    input_queue_get_stats(&stats);
    assert(stats.accepted_push_count == INPUT_EVENT_QUEUE_SIZE);
    assert(stats.overflow_count == 1U);
    assert(stats.current_depth == INPUT_EVENT_QUEUE_SIZE);
    assert(stats.high_water == INPUT_EVENT_QUEUE_SIZE);

    for (uint32_t i = 0; i < 10U; i++) {
        assert(input_queue_poll(&event) == 0);
    }
    input_queue_get_stats(&stats);
    assert(stats.current_depth == INPUT_EVENT_QUEUE_SIZE - 10U);
    assert(stats.high_water == INPUT_EVENT_QUEUE_SIZE);
    assert(stats.overflow_count == 1U);

    assert(input_queue_push(0) == -1);
    input_queue_get_stats(0);
    input_queue_get_stats(&stats);
    assert(stats.overflow_count == 1U);

    input_queue_init();
    input_queue_get_stats(&stats);
    assert(stats.accepted_push_count == 0U);
    assert(stats.overflow_count == 0U);
    assert(stats.current_depth == 0U);
    assert(stats.high_water == 0U);

    puts("input queue telemetry: ok");
    return 0;
}
