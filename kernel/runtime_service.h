#ifndef ARMONIOS_KERNEL_RUNTIME_SERVICE_H
#define ARMONIOS_KERNEL_RUNTIME_SERVICE_H

#include <stdint.h>

/*
 * Deferred runtime work published by hard-IRQ handlers and consumed once the
 * interrupt controller has received EOI. Repeated requests coalesce in the
 * pending bitmask; the single consumer performs one pass before EL0 dispatch
 * resumes.
 */
enum {
    RUNTIME_WORK_PERIODIC = 1U << 0,
    RUNTIME_WORK_ALL = RUNTIME_WORK_PERIODIC,
};

/*
 * Work performed by one runtime backend invocation. The backend may report
 * more than once; the service accumulates reports into the current pass.
 * Counts describe completed work, not polling attempts.
 */
typedef struct {
    uint32_t board_input_events;
    uint32_t usb_input_events;
    uint32_t input_events_consumed;
    uint32_t input_queue_depth_after_producers;
    uint32_t input_queue_high_water;
    uint64_t input_queue_overflow_delta;
    uint32_t redraw_count;
    uint32_t network_frames;
} runtime_service_work_report_t;

/*
 * Internal runtime-service telemetry. Durations are generic-counter ticks, not
 * CPU clock cycles. counter_frequency_hz converts them to time when non-zero.
 *
 * The structure is kernel-internal for now. It is deliberately snapshot-based
 * so tests and a later versioned Monitor/sysinfo integration do not depend on
 * mutable globals or partially read counters.
 */
typedef struct {
    uint64_t request_count;
    uint64_t coalesced_request_count;
    uint64_t run_count;
    uint64_t empty_run_count;
    uint64_t requeue_count;
    uint64_t last_duration_ticks;
    uint64_t max_duration_ticks;
    uint64_t total_duration_ticks;
    uint64_t over_budget_count;
    uint64_t counter_frequency_hz;
    uint64_t budget_ticks;

    uint64_t total_board_input_events;
    uint64_t total_usb_input_events;
    uint64_t total_input_events_consumed;
    uint64_t input_queue_overflow_count;
    uint64_t total_redraw_count;
    uint64_t total_network_frames;

    uint32_t last_board_input_events;
    uint32_t max_board_input_events;
    uint32_t last_usb_input_events;
    uint32_t max_usb_input_events;
    uint32_t last_input_events_consumed;
    uint32_t max_input_events_consumed;
    uint32_t max_input_queue_depth;
    uint32_t input_queue_high_water;
    uint32_t last_redraw_count;
    uint32_t max_redraw_count;
    uint32_t last_network_frames;
    uint32_t max_network_frames;

    uint32_t pending_work;
    uint32_t last_work;
} runtime_service_stats_t;

void runtime_service_request(uint32_t work);
uint32_t runtime_service_run_pending(void);
void runtime_service_reset(void);

void runtime_service_configure_timing(uint64_t counter_frequency_hz,
                                      uint64_t budget_ticks);
void runtime_service_report_work(const runtime_service_work_report_t *report);
void runtime_service_get_stats(runtime_service_stats_t *stats);

/*
 * Architecture clock hook used by the service. irq.c provides a weak zero
 * clock for host-only link tests; the AArch64 timer driver provides the strong
 * generic-counter implementation.
 */
uint64_t runtime_service_counter_now(void);

#endif
