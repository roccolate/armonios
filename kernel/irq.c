#include "kernel/irq.h"

#include <stdint.h>

#include "board.h"
#include "kernel/process.h"
#include "kernel/runtime_service.h"
#include "uart/pl011.h"

#define IRQ_HANDLER_SLOTS 64U

/*
 * EL1 IRQ dispatch table.
 *
 * Board code owns interrupt-controller acknowledge/end details. This layer only
 * maps interrupt IDs to small callbacks, saves the interrupted EL0 context when
 * a trap frame is present, closes the hard-IRQ phase, runs one deferred runtime
 * service pass, and then gives the scheduler one preemption point.
 */

typedef struct {
    irq_handler_fn_t handler;
    void *context;
} irq_handler_entry_t;

static irq_handler_entry_t g_irq_handlers[IRQ_HANDLER_SLOTS];
static volatile uint32_t g_runtime_work_pending;
static runtime_service_stats_t g_runtime_stats;

/*
 * The normal kernel provides strong implementations in kernel.c and timer.c.
 * Host IRQ tests intentionally omit those translation units, so these weak
 * hooks keep the IRQ layer independently testable and may be overridden by the
 * test binary.
 */
__attribute__((weak)) void kernel_on_timer_tick(void) {
}

__attribute__((weak)) uint64_t runtime_service_counter_now(void) {
    return 0;
}

static void runtime_service_clear_last_work_metrics(void) {
    g_runtime_stats.last_board_input_events = 0;
    g_runtime_stats.last_usb_input_events = 0;
    g_runtime_stats.last_input_events_consumed = 0;
    g_runtime_stats.last_redraw_count = 0;
    g_runtime_stats.last_network_frames = 0;
}

static void update_max_u32(uint32_t value, uint32_t *maximum) {
    if (maximum != 0 && value > *maximum) {
        *maximum = value;
    }
}

void runtime_service_reset(void) {
    uint64_t counter_frequency_hz = g_runtime_stats.counter_frequency_hz;
    uint64_t budget_ticks = g_runtime_stats.budget_ticks;

    g_runtime_work_pending = 0;
    g_runtime_stats = (runtime_service_stats_t){0};
    g_runtime_stats.counter_frequency_hz = counter_frequency_hz;
    g_runtime_stats.budget_ticks = budget_ticks;
}

void runtime_service_configure_timing(uint64_t counter_frequency_hz,
                                      uint64_t budget_ticks) {
    g_runtime_stats.counter_frequency_hz = counter_frequency_hz;
    g_runtime_stats.budget_ticks = budget_ticks;
}

void runtime_service_report_work(const runtime_service_work_report_t *report) {
    if (report == 0) {
        return;
    }

    g_runtime_stats.last_board_input_events += report->board_input_events;
    g_runtime_stats.total_board_input_events += report->board_input_events;
    update_max_u32(g_runtime_stats.last_board_input_events,
                   &g_runtime_stats.max_board_input_events);

    g_runtime_stats.last_usb_input_events += report->usb_input_events;
    g_runtime_stats.total_usb_input_events += report->usb_input_events;
    update_max_u32(g_runtime_stats.last_usb_input_events,
                   &g_runtime_stats.max_usb_input_events);

    g_runtime_stats.last_input_events_consumed +=
        report->input_events_consumed;
    g_runtime_stats.total_input_events_consumed +=
        report->input_events_consumed;
    update_max_u32(g_runtime_stats.last_input_events_consumed,
                   &g_runtime_stats.max_input_events_consumed);

    update_max_u32(report->input_queue_depth_after_producers,
                   &g_runtime_stats.max_input_queue_depth);
    update_max_u32(report->input_queue_high_water,
                   &g_runtime_stats.input_queue_high_water);
    g_runtime_stats.input_queue_overflow_count +=
        report->input_queue_overflow_delta;

    g_runtime_stats.last_redraw_count += report->redraw_count;
    g_runtime_stats.total_redraw_count += report->redraw_count;
    update_max_u32(g_runtime_stats.last_redraw_count,
                   &g_runtime_stats.max_redraw_count);

    g_runtime_stats.last_network_frames += report->network_frames;
    g_runtime_stats.total_network_frames += report->network_frames;
    update_max_u32(g_runtime_stats.last_network_frames,
                   &g_runtime_stats.max_network_frames);
}

void runtime_service_get_stats(runtime_service_stats_t *stats) {
    if (stats == 0) {
        return;
    }

    *stats = g_runtime_stats;
    stats->pending_work = g_runtime_work_pending;
}

void runtime_service_request(uint32_t work) {
    uint32_t accepted = work & RUNTIME_WORK_ALL;

    if (accepted == 0U) {
        return;
    }

    g_runtime_stats.request_count++;
    if ((g_runtime_work_pending & accepted) != 0U) {
        g_runtime_stats.coalesced_request_count++;
    }

    g_runtime_work_pending |= accepted;
}

uint32_t runtime_service_run_pending(void) {
    uint32_t work = g_runtime_work_pending;
    uint64_t started;
    uint64_t finished;
    uint64_t duration;

    /* Clear before dispatch so work published by the backend is preserved for
     * the next pass instead of being erased on return. Hard IRQs remain masked
     * throughout this post-EOI bottom half, so the snapshot itself is atomic on
     * the current single-core runtime. */
    g_runtime_work_pending = 0;
    g_runtime_stats.last_work = work;
    runtime_service_clear_last_work_metrics();

    if (work == 0U) {
        g_runtime_stats.empty_run_count++;
        return 0U;
    }

    started = runtime_service_counter_now();

    if ((work & RUNTIME_WORK_PERIODIC) != 0U) {
        kernel_on_timer_tick();
    }

    finished = runtime_service_counter_now();
    duration = finished - started;

    g_runtime_stats.run_count++;
    g_runtime_stats.last_duration_ticks = duration;
    g_runtime_stats.total_duration_ticks += duration;
    if (duration > g_runtime_stats.max_duration_ticks) {
        g_runtime_stats.max_duration_ticks = duration;
    }
    if (g_runtime_stats.budget_ticks != 0U &&
        duration > g_runtime_stats.budget_ticks) {
        g_runtime_stats.over_budget_count++;
    }
    if (g_runtime_work_pending != 0U) {
        g_runtime_stats.requeue_count++;
    }

    return work;
}

int irq_register_handler(uint32_t irq, irq_handler_fn_t handler, void *context) {
    if (irq >= IRQ_HANDLER_SLOTS || handler == 0) {
        return -1;
    }

    g_irq_handlers[irq].handler = handler;
    g_irq_handlers[irq].context = context;

    return 0;
}

void irq_unregister_handler(uint32_t irq) {
    if (irq >= IRQ_HANDLER_SLOTS) {
        return;
    }

    g_irq_handlers[irq].handler = 0;
    g_irq_handlers[irq].context = 0;
}

void irq_handler_frame(exception_frame_t *frame) {
    process_t *current = process_current();
    uint32_t irq = board_irq_ack();

    if (current != 0 && frame != 0) {
        process_save_context(current, frame->x, frame->elr, frame->spsr,
                             frame->sp_el0);
    }

    if (board_irq_is_spurious(irq)) {
        return;
    }

    if (irq < IRQ_HANDLER_SLOTS && g_irq_handlers[irq].handler != 0) {
        g_irq_handlers[irq].handler(g_irq_handlers[irq].context);
    } else {
        uart_puts("IRQ unknown\n");
    }

    board_irq_end(irq);

    /* Device polling, GUI routing/redraw, and network polling requested by the
     * timer now run only after EOI through this single deferred service. */
    (void)runtime_service_run_pending();

    if (current != 0 && frame != 0) {
        current->pc = frame->elr;
        current->pstate = frame->spsr;
        current->sp = frame->sp_el0;

        (void)process_dispatch_next(current, frame, PROCESS_DISPATCH_PREEMPT);
    }
}

void irq_handler(void) {
    irq_handler_frame(0);
}
