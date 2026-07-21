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

/*
 * The normal kernel provides the strong implementation in kernel.c. Host IRQ
 * tests intentionally omit kernel.c, so this weak no-op keeps the IRQ layer
 * independently testable and can be overridden by the test binary.
 */
__attribute__((weak)) void kernel_on_timer_tick(void) {
}

void runtime_service_reset(void) {
    g_runtime_work_pending = 0;
}

void runtime_service_request(uint32_t work) {
    g_runtime_work_pending |= work & RUNTIME_WORK_ALL;
}

uint32_t runtime_service_run_pending(void) {
    uint32_t work = g_runtime_work_pending;

    /* Clear before dispatch so work published by the backend is preserved for
     * the next pass instead of being erased on return. Hard IRQs remain masked
     * throughout this post-EOI bottom half, so the snapshot itself is atomic on
     * the current single-core runtime. */
    g_runtime_work_pending = 0;

    if ((work & RUNTIME_WORK_PERIODIC) != 0) {
        kernel_on_timer_tick();
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
