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
       -> poll virtio/USB input producers
       -> drain input queue into GUI routing
       -> flush one dirty redraw when needed
       -> drain available virtio-net RX frames
       -> end active measurement pass
       -> read CNTPCT_EL0 and update duration telemetry
  -> process_dispatch_next()
  -> eret
```

EOI completes the interrupt-controller transaction; it does not return from the
CPU exception. During the service pass execution remains in EL1, the exception
frame remains on the EL1 stack, the vector's IRQ-masked state is preserved by
the nested IRQ helpers, and EL0 remains paused.

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

## Timing telemetry

Production timing uses the AArch64 physical generic counter:

- `CNTPCT_EL0`: pass start/end values;
- `CNTFRQ_EL0`: conversion frequency.

`timer_init()` sets the current observation threshold to one timer interval:

```text
threshold_ticks = CNTFRQ_EL0 / timer_hz
```

At 100 Hz this is approximately 10 ms. Exceeding it means one service pass used
more than a complete nominal timer interval. It is an observation threshold, not
the final accepted latency budget.

The internal snapshot records requests, coalescing, non-empty/empty passes,
requeues, last/max/total duration, interval overruns, counter frequency,
threshold, pending work, and last-consumed work.

## Per-class work telemetry

Compact indexed classes currently are:

```text
RUNTIME_METRIC_INPUT_PRODUCED
RUNTIME_METRIC_INPUT_CONSUMED
RUNTIME_METRIC_REDRAW
RUNTIME_METRIC_NETWORK_FRAMES
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
measurement exposes the unbounded behavior that the future budget must limit.

### Input queue pressure

The shared 64-event queue records current depth, lifetime high-water, and
rejected full-queue pushes. During an active pass, successful pops report the
depth before removal, high-water, and cumulative overflow. Runtime telemetry
retains maximum observed depth, high-water, and overflow.

Overflow is explicit evidence rather than silent loss, but the implementation
does not yet prevent it under sustained producer pressure.

### Redraw

`RUNTIME_METRIC_REDRAW` counts successful QEMU display redraw submissions during
the active pass. It does not yet count damage rectangles, full-screen fallback,
pixels, or GPU completion latency.

### Network frames

`RUNTIME_METRIC_NETWORK_FRAMES` increments for each valid virtio-net RX frame
returned by `virtio_net_recv()` during the active runtime pass. The indexed
snapshot therefore exposes frames last pass, maximum frames in one pass, and
cumulative frames since reset.

This measures completed frame consumption, not calls to `net_poll()`. Frames
received outside the active runtime pass do not count.

The current virtio-net interface does not expose a reliable device-drop,
overwrite, or receive-ring overflow counter. The driver has 16 RX descriptors,
but this metric must not be interpreted as proof that no frame was dropped
before software consumed it. Receive drops and hardware-ring pressure remain an
explicit evidence gap.

## Snapshot and reset contract

`runtime_service_get_stats()` copies one kernel-internal snapshot.
`runtime_service_reset()` clears pending work and measured counters while
preserving counter frequency and timing threshold.

All state remains zero-initialized, preserving `.data == 0`. No syscall or
user-visible structure exposes the internal layout. Any later Monitor integration
must define a deliberate versioned diagnostic ABI.

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
- Input produced, input consumed, successful redraws, and consumed network
  frames are measurable as last/max/total values.
- Input queue depth, high-water, and overflow are measurable.
- Reports outside the active bottom-half pass are ignored.
- Instrumentation preserves `.data == 0`, the kernel size ceiling, and user ABI.

## Guarantees not implemented

ArmoniOS does not yet guarantee:

- maximum service or interrupt-to-EL0 latency;
- maximum input events or network frames per pass;
- bounded USB/device polling;
- bounded redraw or damage work;
- measured network device drops or RX-ring overflow;
- fairness among work classes and EL0;
- no queue overflow under sustained load;
- pending-bit preservation after future budget exhaustion;
- SMP-safe publication or snapshots;
- a separately schedulable runtime thread.

Measurement does not close `RISK-017`.

## Verification

`tests/run_runtime_service_test.sh` verifies with a deterministic counter:

- request coalescing and EOI-before-backend order;
- requeue preservation;
- last/max/total timing and interval overruns;
- indexed input, redraw, and network last/max/total accumulation;
- inactive report rejection;
- queue pressure accumulation;
- reset and snapshot behavior;
- static wiring of network frame reporting in `virtio_net_recv()`.

`tests/run_input_queue_stats_test.sh` verifies zero state, a 64-entry high-water,
full-queue overflow counting, depth reduction while draining, and reset.

The complete matrix also covers build/size, RPi4 fail-closed paths, native host
tests, FAT32 QEMU smoke, usercopy/focus, framebuffer, USB, network, and visible
FAT+GPU wiring.

## Remaining measurement work

Before selecting budgets, add:

- network receive drop/ring-pressure information if it can be measured honestly;
- USB/device operation counts distinct from generated input events;
- damage rectangles and full-redraw fallback counts;
- per-class and global deadline exhaustion counters;
- pending work retained because a budget expired;
- EL0 heartbeat/progress under sustained synthetic load.

## Required budget behavior

The eventual bounded service should enforce measured limits such as:

```text
input consumed      <= N events/pass
input producers     <= N completions/pass
device work         <= N operations/pass
network receive     <= N frames/pass
redraw               <= one bounded damage batch/pass
global time          <= T generic-counter ticks
```

When a class or global deadline expires:

1. stop processing that work;
2. keep or republish its pending bit;
3. return to process dispatch;
4. continue on a later wakeup;
5. count the exhaustion explicitly.

Values must come from measurements and QEMU stress evidence rather than guesswork.

## RISK-017 exit criteria

The risk remains open until deterministic QEMU tests prove EL0 heartbeat under
sustained input and network traffic, combined pressure does not stall dispatch,
budget-exhausted work remains pending, loss is absent or explicitly counted,
maximum pass duration remains below a documented threshold, and existing gates
remain green.

A later scheduler may promote this work into a wakeable EL1 service, but the
current bottom half must first become explicitly bounded and non-blocking.
