# Deferred Runtime Service

## Purpose

The physical timer callback must remain bounded and independent of GUI, device,
and network workload. ArmoniOS therefore publishes periodic work from the timer
handler and consumes it through one centralized runtime service after the board
interrupt controller receives EOI.

This boundary is an important improvement, but its exact semantics matter:

> The service is deferred past the device-specific timer callback and past EOI,
> but it still runs inside the IRQ exception path before EL0 dispatch and before
> `eret`.

It is a post-EOI EL1 bottom half, not a thread, task queue, or independently
preemptible service.

## Current timing

The physical timer is initialized at 100 Hz on the current QEMU path. One
periodic request is therefore normally published every 10 ms.

```text
EL0 process running
  -> physical timer IRQ
  -> AArch64 vector masks IRQ
  -> save 288-byte exception frame on EL1 stack
  -> irq_handler_frame()
       -> save current EL0 process context
       -> board_irq_ack()
       -> timer_handle_irq()
            -> increment tick count
            -> advance and write CNTP_CVAL
            -> publish RUNTIME_WORK_PERIODIC
            -> update scheduler counters
       -> board_irq_end()
       -> runtime_service_run_pending()
            -> snapshot and clear pending bits
            -> kernel_on_timer_tick()
                 -> poll UART/board/USB input producers
                 -> drain the shared input queue into GUI routing
                 -> flush dirty GUI redraw
                 -> poll network
       -> process_dispatch_next()
  -> restore selected trap frame
  -> eret
  -> selected EL0 process resumes
```

## What EOI changes—and what it does not

`board_irq_end()` tells the interrupt controller that the acknowledged interrupt
has been completed. It does not return from the CPU exception.

During the current runtime-service pass:

- the CPU remains in EL1;
- the IRQ exception frame remains on the EL1 stack;
- normal IRQs remain masked by the vector entry;
- EL0 remains paused;
- another normal timer/device IRQ cannot preempt the pass;
- process dispatch has not happened yet.

Therefore the timer callback has bounded work, but the complete
interrupt-to-EL0-return latency is not yet bounded.

## Pending-work model

The current public work bits are:

```text
RUNTIME_WORK_PERIODIC
```

The pending state is a zero-initialized `uint32_t` bitmask.

Publication performs:

```c
pending |= requested & RUNTIME_WORK_ALL;
```

Consumption performs:

```c
snapshot = pending;
pending = 0;
run(snapshot);
```

Clearing before backend execution guarantees that a request published while a
pass runs remains pending for a later pass instead of being erased when the
current pass returns.

Repeated requests for the same bit coalesce. The service represents “work is
needed”, not an exact count of elapsed ticks.

## Current correctness assumptions

The implementation is correct only under the current runtime assumptions:

- single CPU core;
- IRQ entry masks normal IRQs;
- one runtime-service consumer;
- timer IRQ is the current periodic publisher;
- no concurrent SMP writer;
- no requirement to count every timer publication independently.

The pending word is declared `volatile`, but `volatile` does not make the
read-modify-write operation atomic. Before SMP, nested IRQ publication, or
concurrent EL1 publishers are introduced, the state must move to one of:

- explicit IRQ-masked critical sections outside exception context;
- AArch64 atomic read-modify-write operations;
- per-CPU pending state;
- a scheduler-owned work queue with defined producer/consumer synchronization.

## Backend work in the current pass

One `RUNTIME_WORK_PERIODIC` pass currently performs three broad groups:

### Input and device producers

- UART/serial input servicing;
- board input polling where supported;
- directly attached USB HID polling.

The shared input producer queue has a fixed capacity of 64 events.

### GUI work

- drain all available shared input events;
- route mouse and keyboard events;
- update focus, dragging, cursor, and window queues;
- perform a dirty redraw when required.

Per-window GUI queues have a fixed capacity of 32 events. The compositor can
collapse damage to a full-screen redraw when its damage list fills.

### Network work

- poll the small virtio-net stack;
- process enough Ethernet/ARP/IPv4/UDP/DHCP work for the current direct stack.

There is no socket scheduler or application network API.

## Guarantees implemented now

- `kernel/timer/timer.c` does not call UART, board-input, USB-HID, GUI, or network
  backend routines directly.
- The physical timer callback performs fixed accounting/rearm/publication work.
- Runtime work executes only after `board_irq_end()`.
- Repeated periodic requests coalesce.
- Work published during a pass remains pending for the next pass.
- Pending state is zero-initialized, preserving the kernel `.data == 0`
  contract.
- One source file and one consumer define the current periodic boundary.

## Guarantees not implemented

ArmoniOS does not yet guarantee:

- a maximum runtime-service duration;
- a maximum interrupt-to-EL0-return latency;
- a maximum number of input events processed per pass;
- a maximum number of packets processed per pass;
- a maximum amount of USB/device polling per pass;
- a maximum redraw cost per pass;
- fair scheduling among input, GUI, network, and EL0;
- nested IRQ responsiveness while the pass runs;
- no event loss under sustained producer overload;
- SMP-safe publication;
- a separately wakeable or schedulable EL1 service.

These limitations are tracked by `RISK-017`.

## Verification boundary

`tests/run_runtime_service_test.sh` currently proves:

- two requests for the same bit coalesce into one backend call;
- a request published during backend execution survives for the next pass;
- the backend does not run before the mocked EOI;
- the timer source contains no direct calls matching the forbidden runtime
  backend set;
- the timer source publishes `RUNTIME_WORK_PERIODIC`.

The full QEMU matrix proves that framebuffer, input, USB, network, focus,
usercopy, and FAT paths still reach their expected markers with deferred runtime
work active.

The tests do **not** prove:

- real maximum latency;
- ordering under accumulated physical interrupts;
- fairness under sustained traffic;
- exact timer publication counts;
- no queue overflow;
- exception-stack high-water behavior;
- safe concurrent publication.

## Required instrumentation

The next hardening change should expose at least:

- service pass count;
- empty pass count;
- start/end counter values from the architectural counter;
- last, maximum, and cumulative duration;
- input events processed per pass and high-water mark;
- device polls performed per pass;
- packets/frames processed per pass and high-water mark;
- redraw count, full redraw count, and damage high-water mark;
- budget exhaustion counters;
- pending work left after a pass;
- EL0 heartbeat/progress counter observed during stress tests.

Metrics should be available to host tests where possible and exposed through a
small diagnostic kernel or Monitor path only after their ABI is deliberately
designed.

## Required budgets

A bounded pass should have independent budgets, for example:

```text
input events       <= N per pass
device poll work   <= N operations per pass
network receive    <= N frames/packets per pass
GUI redraw         <= one bounded damage batch per pass
time                <= configured counter-tick budget
```

The exact values must come from measurement, not guesswork.

When a budget is exhausted:

1. stop that work class;
2. keep or republish its pending bit;
3. continue only if the remaining global time budget permits;
4. return to process dispatch;
5. resume work on a later tick or wakeup.

Coalescing must remain a readiness signal. If exact event counts matter, the
owning queue or device must retain them independently.

## Stress-test exit criteria

`RISK-017` should not close until deterministic QEMU tests demonstrate:

- an EL0 heartbeat continues under sustained synthetic input;
- an EL0 heartbeat continues under sustained synthetic network traffic;
- simultaneous input/network/redraw load does not stall process dispatch;
- pending work remains set after a budget-exhausted pass;
- queued events are either delivered or overflow is explicitly counted;
- runtime maximum duration remains under a documented threshold;
- existing framebuffer, USB, network, focus, usercopy, and FAT gates remain
  green.

## Future scheduler integration

A later scheduler revision may promote runtime work into a wakeable EL1 service
when the kernel can schedule EL1 work alongside long-lived EL0 processes.

That design should define:

- service priority relative to EL0;
- wakeup semantics;
- blocking and wait queues;
- per-work-class budgets;
- producer synchronization;
- whether device IRQs publish specific work bits;
- stack ownership;
- cancellation and shutdown behavior;
- SMP/per-CPU policy.

Until that scheduler exists, the current bottom half must remain non-blocking,
small, measured, and explicitly bounded.
