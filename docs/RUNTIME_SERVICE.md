# Deferred Runtime Service

## Purpose

ArmoniOS keeps the physical timer callback independent of GUI, device, and
network workload. The timer publishes periodic work and one centralized runtime
service consumes it after the interrupt controller receives EOI.

The exact boundary is:

> deferred past the timer callback and EOI, but still inside the IRQ exception
> path before process dispatch and `eret`.

It is a post-EOI EL1 bottom half, not a thread or independently preemptible work
queue.

## Current flow

The QEMU timer runs at 100 Hz, normally publishing one request every 10 ms.

```text
EL0 process
  -> timer IRQ and 288-byte EL1 exception frame
  -> timer_handle_irq()
       -> account tick
       -> rearm CNTP_CVAL
       -> publish RUNTIME_WORK_PERIODIC
       -> update scheduler counters
  -> board_irq_end()
  -> runtime_service_run_pending()
       -> snapshot and clear pending work
       -> read CNTPCT_EL0
       -> mark one active measurement pass
       -> poll virtio-input and USB HID producers
       -> drain shared input queue into GUI routing
       -> submit one dirty redraw when needed
       -> report redraw, partial-damage, or full-redraw shape
       -> drain available virtio-net RX frames
       -> end active measurement pass
       -> read CNTPCT_EL0 and update duration telemetry
  -> process_dispatch_next()
  -> eret
```

EOI completes the interrupt-controller transaction; it does not return from the
CPU exception. During the service pass execution remains in EL1, the exception
frame remains on the EL1 stack, the vector's IRQ-masked state is preserved by
nested IRQ helpers, and EL0 remains paused.

The timer callback is bounded. The complete exception-to-EL0-return path is
measured but not yet bounded.

## Pending-work model

The current pending bit is:

```text
RUNTIME_WORK_PERIODIC
```

Publication masks unknown bits and ORs accepted work into a zero-initialized
`uint32_t`. Repeated requests coalesce. Consumption clears the snapshot before
running the backend:

```c
snapshot = pending;
pending = 0;
run(snapshot);
```

A request republished by the backend therefore remains pending for the next
pass. The bit is a readiness signal, not an exact timer-tick counter.

One periodic bit is no longer sufficient for budgeting. Phase 2 must split at
least input/GUI, device, and network readiness so budget-exhausted work can be
resumed independently.

## Timing telemetry

Production timing uses the AArch64 physical generic counter:

- `CNTPCT_EL0`: pass start and end values;
- `CNTFRQ_EL0`: conversion frequency.

`timer_init()` sets the observation threshold to one timer interval:

```text
threshold_ticks = CNTFRQ_EL0 / timer_hz
```

At 100 Hz this is approximately 10 ms. Exceeding it means one service pass used
more than a complete nominal timer interval. It is an observation threshold, not
the final accepted latency budget.

The internal snapshot records requests, coalescing, non-empty and empty passes,
requeues, last/maximum/cumulative duration, interval overruns, counter frequency,
threshold, pending work, and last-consumed work.

## Per-class work telemetry

Compact indexed classes are:

```text
RUNTIME_METRIC_INPUT_PRODUCED
RUNTIME_METRIC_INPUT_CONSUMED
RUNTIME_METRIC_REDRAW
RUNTIME_METRIC_NETWORK_FRAMES
RUNTIME_METRIC_DEVICE_POLLS
RUNTIME_METRIC_DAMAGE_ITEMS
RUNTIME_METRIC_FULL_REDRAWS
```

Each class records:

- work reported by the last non-empty pass;
- maximum work reported by any pass;
- cumulative work reported since reset.

Reports are accepted only while `runtime_service_run_pending()` has marked the
backend active. Work performed by the cooperative console thread is ignored, so
it cannot contaminate bottom-half measurements.

### Input produced

`RUNTIME_METRIC_INPUT_PRODUCED` counts successful events returned by QEMU
virtio-input and directly attached USB HID keyboard/mouse polling. UART input is
interrupt-driven and is not part of this producer metric.

### Input consumed

`RUNTIME_METRIC_INPUT_CONSUMED` increments for every successful shared queue pop
during the active runtime pass. The GUI path still drains until empty, so this
measurement exposes the unbounded behavior that the future input budget must
limit.

### Input queue pressure

The shared 64-event queue records current depth, lifetime high-water, and
rejected full-queue pushes. During an active pass, successful pops report depth
before removal, high-water, and cumulative overflow. Runtime telemetry retains
maximum observed depth, high-water, and overflow.

Overflow is explicit evidence rather than silent loss, but it is not prevented
under sustained producer pressure.

### USB HID device polling

`RUNTIME_METRIC_DEVICE_POLLS` increments once for each valid HID device poll that
reaches the active xHCI controller. Invalid devices, missing endpoints, and a
missing controller are not counted.

This metric is intentionally separate from input production. A keyboard or
mouse poll may consume controller work while producing zero events.

The current driver can register at most four direct HID devices and claims no hub
support.

### Network frames

`RUNTIME_METRIC_NETWORK_FRAMES` increments for each valid non-empty virtio-net RX
frame returned by `virtio_net_recv()` during the active runtime pass. The metric
counts completed frame consumption, not calls to `net_poll()`.

The current virtio-net interface has 16 RX descriptors but exposes no trustworthy
device counter for frames dropped, overwritten, or never delivered to software.
Consumed-frame counts must not be presented as proof that the receive path lost
nothing.

### Redraw submissions

`RUNTIME_METRIC_REDRAW` counts successful QEMU display redraw submissions during
the active pass.

### Partial damage batches

`RUNTIME_METRIC_DAMAGE_ITEMS` records the number of merged partial-damage
rectangles attached to a successful redraw. The compositor supports up to 32
rectangles before collapsing to a full-redraw sentinel.

The metric counts the batch shape after merging and clipping, not every original
damage request.

### Full redraws

`RUNTIME_METRIC_FULL_REDRAWS` increments when a successful redraw uses the full
sentinel. It is separate from partial damage because the full sentinel clears
`damage_count`.

Pixels, damaged area, GPU completion latency, and CPU work spent on a failed GPU
submission are not currently measured.

## Snapshot and reset contract

`runtime_service_get_stats()` copies one kernel-internal snapshot.
`runtime_service_reset()` clears pending work and measured counters while
preserving counter frequency and timing threshold.

All state remains zero-initialized, preserving `.data == 0`. No syscall or
user-visible structure exposes the internal layout. Any Monitor integration must
define a deliberate versioned diagnostic ABI.

## Current correctness assumptions

The implementation assumes one CPU core, one runtime-service consumer,
state-preserving nested IRQ helpers, no concurrent SMP writer, and coalesced
readiness rather than exact timer publication counts.

The pending word and telemetry are not SMP-safe atomic structures. Before SMP or
concurrent publishers they require atomics, per-CPU state, or a scheduler-owned
queue.

## Guarantees implemented

- The physical timer callback contains no input, GUI, USB, or network backend
  calls.
- Runtime work begins only after `board_irq_end()`.
- Requests coalesce and backend requeue survives.
- Aggregate generic-counter duration and interval overruns are measured.
- Input produced, input consumed, USB HID polls, consumed network frames, redraw
  submissions, partial damage, and full redraws are measurable as last/max/total
  values.
- Input queue depth, high-water, and overflow are measurable.
- Reports outside the active bottom-half pass are ignored.
- Instrumentation preserves `.data == 0`, the 108000-byte kernel ceiling, and the
  existing user ABI.

## Guarantees not implemented

ArmoniOS does not yet guarantee:

- maximum service or interrupt-to-EL0 latency;
- a maximum number of input events per pass;
- a maximum number of network frames per pass;
- a maximum number of USB HID polls per pass;
- a maximum redraw or damage batch per pass;
- a global generic-counter deadline;
- preservation of class-specific work after budget exhaustion;
- measured device-level network drops or RX-ring overflow;
- fairness among work classes and EL0;
- no queue overflow under sustained load;
- SMP-safe publication or snapshots;
- a separately schedulable runtime thread.

Measurement does not close `RISK-017`.

## Verification

`tests/run_runtime_service_test.sh` verifies with a deterministic counter:

- request coalescing and EOI-before-backend order;
- requeue preservation;
- last/max/total timing and interval overruns;
- indexed last/max/total accumulation for every current work class;
- inactive report rejection;
- queue pressure accumulation;
- direct execution of the redraw helper for partial and full states;
- reset and snapshot behavior;
- static timer, network, USB, and redraw wiring boundaries.

`tests/run_input_queue_stats_test.sh` verifies zero state, a 64-entry high-water,
full-queue overflow counting, depth reduction while draining, and reset.

The complete matrix also covers build/size, RPi4 fail-closed paths, native host
tests, FAT32 QEMU smoke, usercopy/focus, framebuffer, USB, network, and visible
FAT+GPU wiring.

The validated Phase 1B head is
`6634c3a6f527433643a56f2c90cc6af8bad62c1d`:

- `Verify ArmoniOS` run `29840410727`: success;
- `CI - Tests` run `29840411044`: success;
- loadable QEMU kernel: 107370 bytes against the 108000-byte ceiling.

## Phase 2 required behavior

A bounded service should enforce independent limits:

```text
input consumed      <= N events/pass
USB HID polling     <= N device operations/pass
network receive     <= N frames/pass
redraw               <= one bounded damage batch/pass
global time          <= T generic-counter ticks
```

When a class or global deadline expires:

1. stop processing that class;
2. keep or republish its specific pending bit;
3. record class or global exhaustion;
4. return to process dispatch;
5. continue on a later wakeup.

Initial values must be justified by fixed queue limits and QEMU stress evidence.
The first recommended cut is network receive: `net_poll()` contains the clearest
unbounded loop, while the virtio RX queue already provides a natural 16-descriptor
upper reference.

## RISK-017 exit criteria

The risk remains open until deterministic QEMU tests prove:

- EL0 heartbeat progress under sustained input and network traffic;
- combined input/network/redraw pressure does not stall dispatch;
- every budget-exhausted class remains pending;
- observable queue loss is absent or explicitly counted;
- maximum service duration remains below a documented final threshold;
- all existing subsystem gates remain green.

A later scheduler may promote this work into a wakeable EL1 service, but the
current bottom half must first become explicitly bounded and non-blocking.
