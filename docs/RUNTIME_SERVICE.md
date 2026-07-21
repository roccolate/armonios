# Deferred Runtime Service

## Purpose

The physical timer callback must remain independent of GUI, device, and network
workload. ArmoniOS therefore publishes periodic work from the timer handler and
consumes it through one centralized runtime service after the board interrupt
controller receives EOI.

This boundary is important, but its exact semantics matter:

> The service is deferred past the device-specific timer callback and past EOI,
> but it still runs inside the IRQ exception path before EL0 dispatch and before
> `eret`.

It is a post-EOI EL1 bottom half, not a thread, task queue, or independently
preemptible service.

## Current flow

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
            -> read CNTPCT_EL0 start value
            -> kernel_on_timer_tick()
                 -> poll UART/board/USB input producers
                 -> drain the shared input queue into GUI routing
                 -> flush dirty GUI redraw
                 -> poll network
            -> read CNTPCT_EL0 end value
            -> update runtime-service telemetry
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

The timer callback has bounded work. The complete interrupt-to-EL0-return
latency remains unbounded until per-pass work budgets land.

## Pending-work model

The current work bits are:

```text
RUNTIME_WORK_PERIODIC
```

The pending state is a zero-initialized `uint32_t` bitmask. Publication masks
unknown bits, counts the accepted request, records whether an already-pending
bit coalesced, and ORs the accepted bits into the pending word.

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
needed”, not an exact count of elapsed timer ticks. The request and coalescing
telemetry make that loss of tick multiplicity observable without changing the
readiness semantics.

## Timing source and observation threshold

The production clock hook reads the AArch64 physical generic counter through
`CNTPCT_EL0`. Durations are recorded in generic-counter ticks, not CPU cycles.
`CNTFRQ_EL0` is recorded so diagnostics can convert ticks to time.

`timer_init()` configures the initial observation threshold to one physical
timer interval:

```text
budget_ticks = CNTFRQ_EL0 / timer_hz
```

At the current 100 Hz configuration, an over-budget pass is one whose measured
duration exceeds approximately 10 ms. This is an observation threshold, not the
final runtime budget. It detects a pass that consumed more than the complete
nominal interval before the next timer deadline. Later work must introduce
smaller independent budgets for input, device, network, and redraw work.

The host test overrides the clock hook with a deterministic counter. The normal
kernel receives the strong implementation from `kernel/timer/timer.c`; host-only
IRQ links fall back to a weak zero clock.

## Telemetry snapshot

`runtime_service_get_stats()` returns a kernel-internal snapshot containing:

- accepted request count;
- coalesced request count;
- non-empty run count;
- empty consumer invocation count;
- passes that left work requeued;
- last, maximum, and cumulative duration ticks;
- passes exceeding the configured observation threshold;
- counter frequency and configured threshold;
- current pending bits;
- work bits consumed by the last invocation.

`runtime_service_reset()` clears pending work and measured counters while
preserving the configured counter frequency and observation threshold. All
telemetry storage is zero-initialized, preserving the kernel `.data == 0`
contract.

The snapshot is internal and does not create a new syscall or user-visible ABI.
A later Monitor or diagnostic integration must deliberately define consistency,
units, versioning, and access semantics.

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

Telemetry counters follow the same current single-core assumption. The snapshot
API is not an SMP-consistent transactional read.

## Backend work in the current pass

One `RUNTIME_WORK_PERIODIC` pass currently performs three broad groups.

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
- process Ethernet/ARP/IPv4/UDP/DHCP work for the current direct stack.

There is no socket scheduler or application network API.

## Guarantees implemented now

- `kernel/timer/timer.c` does not call UART, board-input, USB-HID, GUI, or network
  backend routines directly.
- The physical timer callback performs fixed accounting/rearm/publication work.
- Runtime work executes only after `board_irq_end()`.
- Repeated periodic requests coalesce.
- Work published during a pass remains pending for the next pass.
- Requests, coalescing, runs, empty invocations, requeues, duration, maximum,
  cumulative duration, and interval overruns are measured.
- The timing threshold follows the configured timer interval instead of a
  hardcoded counter frequency.
- Pending and telemetry state are zero-initialized.
- The instrumentation introduces no user ABI and does not change backend work.

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
- SMP-safe publication or telemetry;
- a separately wakeable or schedulable EL1 service.

Measurement does not close `RISK-017`; it supplies the data required for the
budgeting phase.

## Verification boundary

`tests/run_runtime_service_test.sh` proves with a deterministic host counter:

- two requests for the same bit coalesce into one backend call;
- request and coalescing counters are correct;
- last, maximum, and cumulative duration tracking is correct;
- the timer-interval observation threshold increments the over-budget counter;
- a request published during backend execution survives and is counted as a
  requeue;
- reset clears measured state while preserving timing configuration;
- the backend does not run before the mocked EOI;
- the timer source contains no direct calls matching the forbidden runtime
  backend set;
- the timer source publishes `RUNTIME_WORK_PERIODIC`.

The full QEMU matrix proves that framebuffer, input, USB, network, focus,
usercopy, and FAT paths still reach their expected markers with deferred runtime
work active.

The tests do **not** prove:

- the real QEMU or hardware maximum duration;
- fairness under sustained traffic;
- no queue overflow or event loss;
- exception-stack high-water behavior;
- safe concurrent publication;
- that the one-interval threshold is an acceptable final budget.

## Remaining instrumentation

The first timing layer is implemented. The next measurement cut should add
work-class detail:

- input events processed per pass and queue high-water mark;
- device polls or completions per pass;
- packets/frames processed per pass and receive high-water mark;
- redraw count, full redraw count, damage count, and damage high-water mark;
- per-class budget exhaustion counters;
- pending work retained because a class or global budget expired;
- EL0 heartbeat/progress observed during stress tests.

Metrics should remain available to host tests. Any Monitor exposure must use a
small deliberately documented diagnostic ABI rather than leaking the internal
structure directly.

## Required budgets

A bounded pass should have independent limits:

```text
input events       <= measured N per pass
device work        <= measured N operations per pass
network receive    <= measured N frames/packets per pass
GUI redraw         <= one bounded damage batch per pass
global time        <= configured counter-tick budget
```

The values must come from measurement, not guesswork.

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
- runtime maximum duration remains under a documented final threshold;
- existing framebuffer, USB, network, focus, usercopy, and FAT gates remain
  green.

## Future scheduler integration

A later scheduler revision may promote runtime work into a wakeable EL1 service
when the kernel can schedule EL1 work alongside long-lived EL0 processes.

That design should define service priority, wakeup semantics, blocking and wait
queues, per-work-class budgets, producer synchronization, stack ownership,
cancellation, shutdown, and SMP/per-CPU policy.

Until that scheduler exists, the current bottom half must remain non-blocking,
measured, and explicitly bounded before v0.2 promotion.
