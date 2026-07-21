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
       -> poll network
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

The internal snapshot records:

- accepted and coalesced requests;
- non-empty and empty consumer invocations;
- passes that left work requeued;
- last, maximum, and cumulative duration ticks;
- passes exceeding the configured threshold;
- counter frequency and threshold;
- pending and last-consumed work bits.

## Per-class work telemetry

Phase 1B adds compact indexed work classes:

```text
RUNTIME_METRIC_INPUT_PRODUCED
RUNTIME_METRIC_INPUT_CONSUMED
RUNTIME_METRIC_REDRAW
```

For each class the snapshot records:

- work reported by the last non-empty pass;
- maximum work reported by any pass;
- cumulative work reported since reset.

Reports are accepted only while `runtime_service_run_pending()` has marked the
backend active. The same queue and device functions may run from the cooperative
console thread, but work outside the active bottom-half pass is ignored. This
prevents console servicing from contaminating runtime-service measurements.

### Input produced

`RUNTIME_METRIC_INPUT_PRODUCED` counts successful input events returned by:

- QEMU virtio-input polling;
- directly attached USB HID keyboard/mouse polling.

UART input is interrupt-driven and the timer pass calls
`kernel_io_poll_input_sources(0)`, so UART bytes are not part of this producer
metric.

### Input consumed

`RUNTIME_METRIC_INPUT_CONSUMED` increments for each successful shared input-queue
pop during an active service pass. The GUI path currently drains the queue until
empty, so this measurement exposes the exact unbounded behavior that the later
budget must limit.

### Input queue pressure

The shared 64-event input queue now records:

- current depth;
- lifetime high-water mark;
- rejected pushes caused by a full queue.

During an active pass, successful pops report the depth observed before removal,
the global high-water mark, and cumulative overflow count. Runtime telemetry
retains maximum observed depth, high-water, and overflow count.

Overflow is now explicit evidence rather than silent loss. The implementation
still does not prevent overflow under sustained producer pressure.

### Redraw

`RUNTIME_METRIC_REDRAW` counts successful QEMU display redraw submissions made
while the runtime pass is active. It does not yet count damage rectangles,
full-screen fallbacks, pixels, or GPU completion latency.

### Network

Network receive frames are **not yet measured**. `net_poll()` still has a void
interface and may drain multiple frames. Network frame count, receive high-water,
and overflow/drop information remain part of the next measurement cut.

## Snapshot and reset contract

`runtime_service_get_stats()` copies one kernel-internal snapshot.
`runtime_service_reset()` clears pending work and measured counters while
preserving counter frequency and timing threshold.

All state remains zero-initialized, preserving `.data == 0`. No syscall or
user-visible struct exposes the internal layout. A future Monitor integration
must define a deliberate versioned diagnostic ABI rather than copying this
structure directly to EL0.

## Current correctness assumptions

The implementation assumes:

- one CPU core;
- one runtime-service consumer;
- nested IRQ helpers restore the prior DAIF IRQ-mask state;
- no concurrent SMP writer;
- coalesced readiness rather than exact timer publication counts.

The pending word and telemetry counters are not SMP-safe atomic data structures.
Before SMP or concurrent publishers, they require atomics, per-CPU state, or a
scheduler-owned queue.

## Guarantees implemented

- The physical timer callback contains no input, GUI, USB, or network backend
  calls.
- Runtime work begins only after `board_irq_end()`.
- Requests coalesce and backend requeue survives.
- Aggregate generic-counter duration and interval overruns are measured.
- Input produced, input consumed, successful redraws, queue depth/high-water, and
  queue overflow are measurable.
- Lower-layer reports outside the active bottom-half pass are ignored.
- Queue overflow is counted explicitly.
- The instrumentation preserves `.data == 0`, the 108000-byte kernel ceiling,
  and the existing user ABI.

## Guarantees not implemented

ArmoniOS does not yet guarantee:

- maximum service or interrupt-to-EL0 latency;
- a maximum number of input events per pass;
- bounded USB/device polling;
- bounded redraw work;
- measured or bounded network frames;
- fairness among work classes and EL0;
- no queue overflow under sustained load;
- pending-bit preservation after a budget expires, because budgets do not yet
  exist;
- SMP-safe publication or snapshots;
- a separately schedulable runtime thread.

Measurement does not close `RISK-017`.

## Verification

`tests/run_runtime_service_test.sh` uses a deterministic counter and verifies:

- request coalescing;
- EOI-before-backend order;
- requeue preservation;
- last/max/total timing and interval overruns;
- indexed metric last/max/total accumulation;
- inactive report rejection;
- queue pressure accumulation;
- reset and snapshot behavior.

`tests/run_input_queue_stats_test.sh` verifies:

- depth begins at zero;
- filling all 64 entries reaches a high-water mark of 64;
- an additional push is rejected and increments overflow;
- consuming entries decreases current depth without reducing high-water;
- reset clears queue telemetry.

The complete matrix continues to exercise build/size, RPi4 fail-closed paths,
host tests, FAT32 QEMU smoke, usercopy/focus, framebuffer, USB, network, and
visible FAT+GPU wiring.

The compact implementation produces a 107204-byte QEMU kernel binary against the
108000-byte ceiling on the validated candidate build.

## Remaining measurement work

Before selecting budgets, add:

- network frames processed per pass;
- network receive/drop/high-water information;
- USB/device poll or completion counts distinct from generated input events;
- damage rectangles and full-redraw fallback counts;
- per-class budget-exhaustion counters;
- global deadline exhaustion;
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

The risk remains open until deterministic QEMU tests prove:

- EL0 heartbeat progress under sustained input;
- EL0 progress under sustained network traffic;
- combined input/network/redraw pressure does not stall dispatch;
- budget-exhausted work remains pending;
- queued work is delivered or loss is explicitly counted;
- maximum pass duration remains below a documented final threshold;
- all existing subsystem gates remain green.

A later scheduler may promote this work into a wakeable EL1 service, but the
current bottom half must first become explicitly bounded and non-blocking.
