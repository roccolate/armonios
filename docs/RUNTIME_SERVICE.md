# Deferred Runtime Service

## Purpose

The timer hard-IRQ path must remain bounded. Periodic GUI work, device polling,
and network polling are therefore published as deferred work and consumed by one
runtime service after the interrupt controller receives EOI.

## Current flow

```text
physical timer IRQ
  -> account tick
  -> rearm CNTP_CVAL
  -> publish RUNTIME_WORK_PERIODIC
  -> update scheduler counters
  -> return to generic IRQ dispatcher
  -> board_irq_end()
  -> runtime_service_run_pending()
       -> UART/input producer polling
       -> GUI event routing and dirty redraw
       -> network polling
  -> EL0 process dispatch
```

Repeated timer requests coalesce in a bitmask. The consumer clears the snapshot
before invoking the backend, so work published during a pass remains pending for
the next pass instead of being lost.

## Guarantees in this cut

- `kernel/timer/timer.c` does not call UART, GUI, board-input, USB-HID, or network
  polling functions.
- Deferred work runs only after EOI.
- There is one consumer for timer-published runtime work.
- Repeated requests coalesce.
- A request published while the backend runs survives for a later pass.
- The state is zero-initialized, preserving the `.data == 0` contract.

`tests/run_runtime_service_test.sh` verifies coalescing, requeue preservation,
EOI ordering, and the static timer-handler boundary. The gate is part of
`tools/verify.sh`.

## Important limitation

This is a post-EOI EL1 bottom half, not a separately scheduled or preemptible
kernel thread. The current cooperative EL1 scheduler does not interleave helper
threads while the long-lived panel executes in EL0, so moving the work directly
to an EL1 helper thread would stop desktop runtime pumping.

The service must therefore remain bounded and non-blocking. A later scheduler
cut may make it a real wakeable kernel service once EL1 work can be scheduled
alongside active EL0 processes.

## Follow-up risk

The next runtime hardening step should measure maximum service duration and add
budgets for GUI event draining, redraw, USB polling, and network receive work.
That step must prevent starvation by bounding work per pass and preserving any
unconsumed work bit for the next tick.
