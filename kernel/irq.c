#include "kernel/irq.h"

#include <stdint.h>

#include "board.h"
#include "input/input.h"
#include "kernel/gui_compositor.h"
#include "kernel/kstring.h"
#include "net/virtio_net.h"
#include "kernel/net/dhcp.h"
#include "kernel/process.h"
#include "kernel/runtime_service.h"

#define IRQ_HANDLER_SLOTS 64U

/* The low nibble stores batch + 1, so zero means no prepared redraw. */
#define RUNTIME_REDRAW_COUNT_MASK 0x0fU
#define RUNTIME_REDRAW_FULL       (1U << 4)
#define RUNTIME_REDRAW_SUCCESS    (1U << 5)
#define RUNTIME_REDRAW_EXHAUSTED  (1U << 6)
#define RUNTIME_PHASE_DEADLINE    (1U << 3)

typedef struct {
    irq_handler_fn_t handler;
    void *context;
} irq_handler_entry_t;

static irq_handler_entry_t g_irq_handlers[IRQ_HANDLER_SLOTS];
static runtime_service_stats_t g_runtime_stats;
static uint8_t g_runtime_phase;
static uint8_t g_runtime_redraw_state;

__attribute__((weak)) void kernel_on_timer_tick(void) {
}

__attribute__((weak)) void kernel_io_poll_network(void) {
}

__attribute__((weak)) uint64_t runtime_service_counter_now(void) {
    return 0;
}

void runtime_service_reset(void) {
    uint64_t frequency = g_runtime_stats.counter_frequency_hz;
    uint64_t budget = g_runtime_stats.budget_ticks;

    g_runtime_phase = 0;
    g_runtime_redraw_state = 0;
    g_runtime_stats = (runtime_service_stats_t){0};
    g_runtime_stats.counter_frequency_hz = frequency;
    g_runtime_stats.budget_ticks = budget;
}

void runtime_service_configure_timing(uint64_t counter_frequency_hz,
                                      uint64_t budget_ticks) {
    g_runtime_stats.counter_frequency_hz = counter_frequency_hz;
    g_runtime_stats.budget_ticks = budget_ticks;
}

static int runtime_service_deadline(void) {
#if defined(ARMONIOS_TEST) && !defined(ARMONIOS_RUNTIME_DEADLINE_TEST)
    return 0;
#else
    uint8_t phase = g_runtime_phase;

    if ((phase & RUNTIME_PHASE_DEADLINE) != 0U) {
        return 1;
    }
    if (phase == 0U || g_runtime_stats.budget_ticks == 0U ||
        runtime_service_counter_now() < g_runtime_stats.last_duration_ticks) {
        return 0;
    }

    g_runtime_stats.pending_work |= phase;
    g_runtime_phase = RUNTIME_PHASE_DEADLINE;
    g_runtime_stats.over_budget_count++;
    return 1;
#endif
}

void runtime_service_report_metric(uint32_t metric, uint32_t value) {
    uint32_t current;

    if (g_runtime_phase == 0U || metric >= RUNTIME_METRIC_COUNT ||
        value == 0U) {
        return;
    }

    current = g_runtime_stats.metric_last[metric] + value;
    g_runtime_stats.metric_last[metric] = current;
    g_runtime_stats.metric_total[metric] += value;
    if (current > g_runtime_stats.metric_max[metric]) {
        g_runtime_stats.metric_max[metric] = current;
    }
    (void)runtime_service_deadline();
}

void runtime_service_gui_render(struct fb *fb, void *context) {
    gui_desktop_t *desktop;
    uint32_t original_count;
    uint32_t batch;

    if (runtime_service_deadline()) {
        return;
    }

    desktop = gui_desktop();
    if (desktop == 0) {
        g_runtime_redraw_state = 1U;
        gui_render((fb_t *)fb, context);
        return;
    }
    if (desktop->damage_full != 0U) {
        g_runtime_redraw_state = RUNTIME_REDRAW_FULL | 1U;
        gui_render((fb_t *)fb, context);
        return;
    }

    original_count = desktop->damage_count;
    batch = original_count;
    if (batch > RUNTIME_REDRAW_DAMAGE_BUDGET) {
        batch = RUNTIME_REDRAW_DAMAGE_BUDGET;
    }
    g_runtime_redraw_state = (uint8_t)(batch + 1U);
    if (original_count > batch) {
        g_runtime_redraw_state |= RUNTIME_REDRAW_EXHAUSTED;
    }
    desktop->damage_count = batch;
    gui_render((fb_t *)fb, context);
    desktop->damage_count = original_count;
}

void runtime_service_report_redraw(void) {
    uint8_t state = g_runtime_redraw_state;
    uint32_t batch;

    runtime_service_report_metric(RUNTIME_METRIC_REDRAW, 1U);
#if defined(ARMONIOS_TEST)
    if (state == 0U) {
        gui_desktop_t *desktop = gui_desktop();

        if (desktop == 0) {
            return;
        }
        if (desktop->damage_full != 0U) {
            state = RUNTIME_REDRAW_FULL | 1U;
        } else {
            batch = desktop->damage_count;
            if (batch > RUNTIME_REDRAW_DAMAGE_BUDGET) {
                batch = RUNTIME_REDRAW_DAMAGE_BUDGET;
                state = RUNTIME_REDRAW_EXHAUSTED;
            }
            state |= (uint8_t)(batch + 1U);
        }
    }
#else
    if (state == 0U) {
        return;
    }
#endif

    batch = (state & RUNTIME_REDRAW_COUNT_MASK) - 1U;
    if ((state & RUNTIME_REDRAW_FULL) != 0U) {
        runtime_service_report_metric(RUNTIME_METRIC_FULL_REDRAWS, 1U);
    } else {
        runtime_service_report_metric(RUNTIME_METRIC_DAMAGE_ITEMS, batch);
        if ((state & RUNTIME_REDRAW_EXHAUSTED) != 0U) {
            runtime_service_report_metric(
                RUNTIME_METRIC_REDRAW_EXHAUSTIONS, 1U);
        }
    }

    if (g_runtime_redraw_state != 0U) {
        g_runtime_redraw_state = state | RUNTIME_REDRAW_SUCCESS;
    }
}

void runtime_service_gui_clear_dirty(void) {
    gui_desktop_t *desktop = gui_desktop();
    uint8_t state = g_runtime_redraw_state;
    uint32_t batch;
    uint32_t remaining;

    g_runtime_redraw_state = 0U;
    if ((state & RUNTIME_REDRAW_SUCCESS) == 0U || desktop == 0) {
        return;
    }

    batch = (state & RUNTIME_REDRAW_COUNT_MASK) - 1U;
    if ((state & RUNTIME_REDRAW_FULL) != 0U ||
        desktop->damage_count <= batch) {
        gui_clear_dirty();
        return;
    }

    remaining = desktop->damage_count - batch;
    /* kmemcpy walks forward; this overlapping left shift is therefore safe. */
    kmemcpy(desktop->damage_rects, desktop->damage_rects + batch,
            remaining * (uint32_t)sizeof(damage_rect_t));
    desktop->damage_count = remaining;
}

void runtime_service_report_input_queue(uint32_t depth, uint32_t high_water,
                                        uint64_t overflow_count) {
    if (g_runtime_phase == 0U) {
        return;
    }
    if (depth > g_runtime_stats.max_input_queue_depth) {
        g_runtime_stats.max_input_queue_depth = depth;
    }
    if (high_water > g_runtime_stats.input_queue_high_water) {
        g_runtime_stats.input_queue_high_water = high_water;
    }
    if (overflow_count > g_runtime_stats.input_queue_overflow_count) {
        g_runtime_stats.input_queue_overflow_count = overflow_count;
    }
}

void runtime_service_get_stats(runtime_service_stats_t *stats) {
    if (stats != 0) {
        *stats = g_runtime_stats;
    }
}

void runtime_service_request(uint32_t work) {
    uint32_t accepted = work & RUNTIME_WORK_ALL;
    uint32_t pending;

    if (accepted == 0U) {
        return;
    }
    pending = g_runtime_stats.pending_work;
    g_runtime_stats.request_count++;
    if ((pending & accepted) != 0U) {
        g_runtime_stats.coalesced_request_count++;
    }
    g_runtime_stats.pending_work = pending | accepted;
}

static void runtime_service_requeue_budget(uint32_t work, uint64_t *counter) {
    if ((g_runtime_stats.pending_work & work) == 0U) {
        (*counter)++;
        g_runtime_stats.request_count++;
        g_runtime_stats.pending_work |= work;
    }
}

int runtime_service_input_poll(struct input_event *event) {
    if (g_runtime_phase == 0U) {
        return input_queue_poll((input_event_t *)event);
    }
    if ((g_runtime_phase & RUNTIME_WORK_INPUT) == 0U) {
        return -1;
    }
    if (g_runtime_stats.metric_last[RUNTIME_METRIC_INPUT_CONSUMED] <
        RUNTIME_INPUT_EVENT_BUDGET) {
        return input_queue_poll((input_event_t *)event);
    }
    if (input_queue_available() > 0) {
        runtime_service_requeue_budget(
            RUNTIME_WORK_INPUT,
            &g_runtime_stats.input_budget_exhaustion_count);
    }
    return -1;
}

void runtime_service_net_poll(void) {
    if (g_runtime_phase == 0U ||
        (g_runtime_phase & RUNTIME_WORK_NETWORK) != 0U) {
        net_poll();
    }
}

int runtime_service_virtio_net_recv(virtio_net_device_t *device, void *data,
                                    uint32_t max_len) {
    if (g_runtime_phase == 0U) {
        return virtio_net_recv(device, data, max_len);
    }
    if ((g_runtime_phase & RUNTIME_WORK_NETWORK) == 0U) {
        return 0;
    }
    if (g_runtime_stats.metric_last[RUNTIME_METRIC_NETWORK_FRAMES] >=
        RUNTIME_NETWORK_FRAME_BUDGET) {
        runtime_service_requeue_budget(
            RUNTIME_WORK_NETWORK,
            &g_runtime_stats.network_budget_exhaustion_count);
        return 0;
    }
    return virtio_net_recv(device, data, max_len);
}

uint32_t runtime_service_run_pending(void) {
    uint32_t work = g_runtime_stats.pending_work;
    uint64_t started;
    uint64_t duration;

    g_runtime_stats.pending_work = 0;
    g_runtime_stats.last_work = work;
    g_runtime_phase = 0;
    g_runtime_redraw_state = 0;
    for (uint32_t i = 0; i < RUNTIME_METRIC_COUNT; i++) {
        g_runtime_stats.metric_last[i] = 0;
    }

    if (work == 0U) {
        g_runtime_stats.empty_run_count++;
        return 0U;
    }

    started = runtime_service_counter_now();
    g_runtime_stats.last_duration_ticks = started + g_runtime_stats.budget_ticks;
    if ((work & (RUNTIME_WORK_PERIODIC | RUNTIME_WORK_INPUT)) != 0U) {
        g_runtime_phase = (uint8_t)(work &
            (RUNTIME_WORK_PERIODIC | RUNTIME_WORK_INPUT));
        kernel_on_timer_tick();
        (void)runtime_service_deadline();
    }
    if ((work & RUNTIME_WORK_NETWORK) != 0U) {
        if (g_runtime_phase == RUNTIME_PHASE_DEADLINE) {
            g_runtime_stats.pending_work |= RUNTIME_WORK_NETWORK;
        } else {
            g_runtime_phase = RUNTIME_WORK_NETWORK;
            if (!runtime_service_deadline()) {
                kernel_io_poll_network();
                (void)runtime_service_deadline();
            }
        }
    }
    duration = runtime_service_counter_now() - started;
    g_runtime_phase = 0;

    g_runtime_stats.run_count++;
    g_runtime_stats.last_duration_ticks = duration;
    g_runtime_stats.total_duration_ticks += duration;
    if (duration > g_runtime_stats.max_duration_ticks) {
        g_runtime_stats.max_duration_ticks = duration;
    }
    if (g_runtime_stats.pending_work != 0U) {
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
    if (irq < IRQ_HANDLER_SLOTS) {
        g_irq_handlers[irq].handler = 0;
        g_irq_handlers[irq].context = 0;
    }
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
    }

    board_irq_end(irq);
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
