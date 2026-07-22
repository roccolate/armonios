# Deferred Runtime Service

## Purpose

ArmoniOS keeps the physical timer callback independent of GUI, device, and
network workload. The timer publishes readiness and one centralized runtime
service consumes it after the interrupt controller receives EOI.

The exact boundary is:

> deferred past the timer callback and EOI, but still inside the IRQ exception
> path before process dispatch and `eret`.

It is a post-EOI EL1 bottom half, not a thread or independently preemptible work
queue.

## Current flow

The QEMU timer runs at 100 Hz and normally publishes readiness every 10 ms.

```text
EL0 process
  -> timer IRQ and 288-byte EL1 exception frame
  -> timer_handle_irq()
       -> account tick
       -> rearm CNTP_CVAL
       -> publish PERIODIC | INPUT | NETWORK
       -> update scheduler counters
  -> board_irq_end()
  -> runtime_service_run_pending()
       -> snapshot and clear pending readiness
       -> read CNTPCT_EL0
       -> mark active service pass
       -> PERIODIC backend with INPUT phase enabled
            -> poll virtio-input and USB HID producers
            -> consume at most 16 shared queue events
            -> route consumed events to GUI
            -> submit dirty redraw and report batch shape
            -> inherited network call suppressed
       -> NETWORK phase
            -> run network poll explicitly
            -> consume at most 16 valid RX frames
            -> conservatively republish NETWORK at the cap
       -> end active pass
       -> update duration and exhaustion telemetry
  -> process_dispatch_next()
  -> eret
```

EOI completes the interrupt-controller transaction; it does not return from the
CPU exception. During the service pass execution remains in EL1, the exception
frame remains on the EL1 stack, nested IRQ helpers preserve the vector's masked
state, and EL0 remains paused.

The physical timer callback is bounded. Input queue consumption and network frame
consumption now have count bounds. The complete exception-to-EL0-return path is
not yet globally bounded.

## Pending-work model

The pending word accepts:

```text
RUNTIME_WORK_PERIODIC
RUNTIME_WORK_INPUT
RUNTIME_WORK_NETWORK
```

Publication masks unknown bits and ORs accepted readiness into a zero-initialized
`uint32_t`. Repeated requests coalesce. Consumption clears the snapshot before
running the backend:

```c
snapshot = pending;
pending = 0;
run(snapshot);
```

A request republished by the backend survives for the next pass. Bits represent
readiness, not exact timer or device-event counts.

Input and network are independently resumable. Input producer/USB work and GUI
redraw still execute through the periodic backend; later cuts must split them
where independent continuation is required.

## Timing telemetry

Production timing uses the AArch64 physical generic counter:

- `CNTPCT_EL0`: pass start and end;
- `CNTFRQ_EL0`: conversion frequency.

`timer_init()` configures one timer interval as the observation threshold:

```text
threshold_ticks = CNTFRQ_EL0 / timer_hz
```

At 100 Hz this is approximately 10 ms. Exceeding it means one service pass used
more than a nominal timer interval. It is not the final global latency budget.

The internal snapshot records requests, coalescing, non-empty/empty passes,
requeues, duration values, interval overruns, counter frequency, threshold,
pending work, and last-consumed work.

## Per-class telemetry

Indexed classes are:

```text
RUNTIME_METRIC_INPUT_PRODUCED
RUNTIME_METRIC_INPUT_CONSUMED
RUNTIME_METRIC_REDRAW
RUNTIME_METRIC_NETWORK_FRAMES
RUNTIME_METRIC_DEVICE_POLLS
RUNTIME_METRIC_DAMAGE_ITEMS
RUNTIME_METRIC_FULL_REDRAWS
```

Each stores last-pass, maximum-pass, and cumulative counts. Reports are accepted
only while the runtime backend is active, so cooperative console-thread work does
not contaminate bottom-half class measurements.

### Input and queue pressure

- `INPUT_PRODUCED` counts virtio-input and direct USB HID events.
- `INPUT_CONSUMED` counts successful shared-queue pops during the active input
  phase.
- The 64-event queue records current depth, lifetime high-water, and rejected
  full-queue pushes.

Queue overflow is explicit evidence rather than silent loss, but it is not
prevented under sustained producer pressure.

### USB HID polling

`DEVICE_POLLS` increments once for each valid HID poll that reaches the active
xHCI controller. It is separate from produced input because a poll may perform
controller work while yielding no event.

USB and other input producer polling are measured but not yet limited.

### Network frames

`NETWORK_FRAMES` increments for each valid non-empty virtio-net RX frame returned
during the active network phase. It counts completed software consumption, not
calls to `net_poll()`.

The current virtio-net path has 16 RX descriptors and no trustworthy device-drop,
overwrite, or ring-overflow counter. Consumed-frame counts are not proof of
loss-free receive.

### Redraw and damage

- `REDRAW` counts successful QEMU display submissions.
- `DAMAGE_ITEMS` counts merged partial-damage rectangles attached to a successful
  redraw.
- `FULL_REDRAWS` counts successful submissions using the full-redraw sentinel.

Pixels, damaged area, GPU completion latency, and failed-submission CPU work are
not measured. Redraw work remains unbounded.

## Enforced input budget

The second Phase 2 bound is:

```text
RUNTIME_INPUT_EVENT_BUDGET == 16
```

The value is one quarter of the fixed 64-event shared queue.

Kernel orchestration routes `input_queue_poll()` through
`runtime_service_input_poll()`:

- during an active service pass outside the input phase, queue consumption is
  suppressed;
- during the input phase, successful pops are allowed until 16 have completed;
- outside the active service, the wrapper passes directly to the real queue poll.

At the cap the wrapper calls `input_queue_available()` without consuming:

1. if the queue is empty, the phase finishes without requeue or exhaustion;
2. if events remain, `input_budget_exhaustion_count` increments;
3. `RUNTIME_WORK_INPUT` is republished;
4. the current queue drain stops;
5. process dispatch resumes after the service returns;
6. input continuation occurs on a later pass.

Consequences:

- exactly 16 events with an empty queue do not schedule an empty follow-up;
- a seventeenth queued event is completed later;
- exhaustion means the cap was reached while queue work remained;
- console-thread and other outside-service consumers retain prior behavior.

The runtime backend is not duplicated when normal timer work contains both
`PERIODIC` and `INPUT`: the existing `kernel_on_timer_tick()` backend executes
once with the input phase enabled.

## Enforced network budget

The first Phase 2 bound is:

```text
RUNTIME_NETWORK_FRAME_BUDGET == 16
```

During an active post-EOI network phase:

1. valid frames are consumed until 16 have completed;
2. the next receive attempt returns no frame to the DHCP loop;
3. `network_budget_exhaustion_count` increments once;
4. `RUNTIME_WORK_NETWORK` is republished;
5. network continuation occurs on a later pass.

The network requeue is deliberately conservative. A non-consuming RX query
crossed a 2 KiB linker-alignment boundary and exceeded the kernel ceiling.
Therefore exactly 16 frames may schedule one empty follow-up pass.

The bound applies only to the active runtime network phase. Cooperative network
polling outside the service remains unbudgeted.

## Snapshot and reset contract

`runtime_service_get_stats()` copies one kernel-internal snapshot.
`runtime_service_reset()` clears pending readiness and measured counters while
preserving counter frequency and timing threshold.

The snapshot includes separate input- and network-budget exhaustion counts.
All state remains zero-initialized, preserving `.data == 0`. No syscall exposes
the internal layout. Any Monitor integration requires a versioned diagnostic ABI.

## Current correctness assumptions

The implementation assumes:

- one CPU core;
- one runtime-service consumer;
- state-preserving nested IRQ helpers;
- no concurrent SMP writer;
- coalesced readiness rather than exact publication counts.

The pending word and telemetry are not SMP-safe atomic structures. SMP requires
atomics, per-CPU state, or a scheduler-owned queue.

## Guarantees implemented

- The physical timer callback contains no input, GUI, USB, or network backend
  calls.
- Runtime work begins only after `board_irq_end()`.
- Periodic, input, and network readiness are independent.
- Requests coalesce and backend requeue survives.
- Aggregate duration and all current class metrics are measurable.
- Active input consumption is limited to 16 events per pass.
- Input continuation is republished only when queue work remains.
- Active network RX is limited to 16 valid frames per pass.
- Reaching the network cap conservatively republishes readiness.
- Input and network exhaustion are counted separately.
- Input queue depth, high-water, and overflow are measurable.
- Reports outside the active bottom half are ignored.
- `.data == 0`, the 108000-byte kernel ceiling, and the user ABI are preserved.

## Guarantees not implemented

ArmoniOS does not yet guarantee:

- maximum total service or interrupt-to-EL0 latency;
- maximum input producer or USB HID operations per pass;
- maximum redraw or damage work per pass;
- a global generic-counter deadline;
- fairness among all work classes and EL0;
- no input queue overflow under sustained load;
- measured device-level network drops;
- bounded cooperative input or network work outside the runtime service;
- SMP-safe publication or snapshots;
- a separately schedulable runtime thread.

The two count budgets progress but do not close `RISK-017`.

## Verification

`tests/run_runtime_service_test.sh` builds and executes both the historical
runtime regression and `tests/runtime_input_budget_test.c`.

Coverage includes:

- EOI-before-backend order;
- request coalescing, timing, generic requeue, reset, and all metrics;
- exactly 16 input events completing without requeue;
- 17 input events split across two passes;
- one backend call for combined periodic/input readiness;
- input consumption outside the service remaining unbudgeted;
- exactly 16 network frames followed by conservative recheck;
- 17 network frames split across two passes;
- network polling outside the service remaining unbudgeted;
- static timer, input, network, USB, and redraw wiring.

The shorter hosted workflow runs this script with strict `pipefail` and uploads a
`runtime-service-test-log` artifact.

Validated input-budget head:
`ba8051cd8edbe6a66a843f80c54c96668d064a91`.

- `Verify ArmoniOS` run `29853659559`: success;
- `CI - Tests` run `29853659491`: success;
- loadable QEMU kernel: 107802 / 108000 bytes;
- remaining margin: 198 bytes;
- merge: `41f3e185ca1f75ed09416313d34279384f3d78a9`.

## Remaining Phase 2 behavior

The remaining bounded service should enforce:

```text
input/USB producers   <= N operations/pass
redraw/damage         <= bounded batch/pass
global time           <= T generic-counter ticks
```

For every class or deadline exhaustion:

1. stop processing that work;
2. preserve or republish its specific pending bit;
3. record exhaustion;
4. return toward process dispatch;
5. continue on a later wakeup.

Only 198 bytes remain below the current kernel ceiling. The next cut should first
compact shared runtime phase state and telemetry, then add the next bound without
raising the limit.

## RISK-017 exit criteria

The risk remains open until deterministic and sustained-load QEMU tests prove:

- EL0 heartbeat progress under input, USB, network, and redraw pressure;
- combined work cannot indefinitely delay dispatch;
- every exhausted class remains pending;
- observable queue loss is absent or explicitly counted;
- maximum complete-pass duration stays below a documented final threshold;
- all existing subsystem gates remain green.

A later scheduler may promote this work into a wakeable EL1 service, but the
current bottom half must first become completely bounded and non-blocking.
