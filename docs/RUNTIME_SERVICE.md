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

The QEMU timer runs at 100 Hz, normally publishing readiness every 10 ms.

```text
EL0 process
  -> timer IRQ and 288-byte EL1 exception frame
  -> timer_handle_irq()
       -> account tick
       -> rearm CNTP_CVAL
       -> publish PERIODIC | NETWORK readiness
       -> update scheduler counters
  -> board_irq_end()
  -> runtime_service_run_pending()
       -> snapshot and clear pending readiness
       -> read CNTPCT_EL0
       -> mark active service pass
       -> PERIODIC phase
            -> virtio-input and USB HID producers
            -> shared input queue to GUI
            -> one dirty redraw plus batch telemetry
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

The physical timer callback is bounded. The post-EOI network frame count is now
bounded. The complete exception-to-EL0-return path is not yet globally bounded.

## Pending-work model

The pending word currently accepts:

```text
RUNTIME_WORK_PERIODIC
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

Network is the first independently resumable class. Input, USB, and GUI work
still share `RUNTIME_WORK_PERIODIC`; later cuts must split them where independent
continuation is required.

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
- `INPUT_CONSUMED` counts successful shared-queue pops during the active pass.
- The 64-event queue records current depth, lifetime high-water, and rejected
  full-queue pushes.

The GUI path still drains input until empty. Overflow is counted but not
prevented; input work is not yet bounded.

### USB HID polling

`DEVICE_POLLS` increments once for each valid HID poll that reaches the active
xHCI controller. It is separate from produced input because a poll may perform
controller work while yielding no event.

USB polling is measured but not yet limited.

### Network frames

`NETWORK_FRAMES` increments for each valid non-empty virtio-net RX frame returned
during the active network phase. It counts completed software consumption, not
calls to `net_poll()`.

The current virtio-net path has 16 RX descriptors and no trustworthy device-drop,
overwrite, or ring-overflow counter. Consumed-frame counts must not be presented
as proof of loss-free receive.

### Redraw and damage

- `REDRAW` counts successful QEMU display submissions.
- `DAMAGE_ITEMS` counts merged partial-damage rectangles attached to a successful
  redraw.
- `FULL_REDRAWS` counts successful submissions using the full-redraw sentinel.

Pixels, damaged area, GPU completion latency, and failed-submission CPU work are
not measured. Redraw work remains unbounded.

## Enforced network budget

The first Phase 2 bound is:

```text
RUNTIME_NETWORK_FRAME_BUDGET == 16
```

During an active post-EOI network phase:

1. valid frames are consumed normally until 16 have completed;
2. the next receive attempt returns no frame to the DHCP loop;
3. `network_budget_exhaustion_count` increments once for that pass;
4. `RUNTIME_WORK_NETWORK` is republished;
5. process dispatch resumes after the service returns;
6. the network class continues on a later pass.

The requeue is deliberately conservative. An earlier design queried the RX queue
at the limit, but retaining that helper crossed a 2 KiB linker-alignment boundary
and exceeded the 108000-byte kernel ceiling. The compact rule preserves work
without raising the limit.

Consequences:

- a seventeenth queued frame is completed later;
- exactly sixteen frames may schedule one empty follow-up network pass;
- exhaustion means the cap was reached, not proof that additional RX work existed;
- no work is silently discarded by the budget wrapper.

The bound applies only while the post-EOI runtime network phase is active.
Cooperative network polling outside the service remains unbudgeted and is not
covered by this class guarantee.

## Routing contract

The kernel orchestrator's existing network call is routed through
`runtime_service_net_poll()`.

- During the active periodic phase, the inherited network call is suppressed.
- During `RUNTIME_WORK_NETWORK`, the wrapper permits the real `net_poll()`.
- Outside the runtime service, cooperative callers continue to use the real poll
  without this budget.

The DHCP receive loop is compiled through `runtime_service_virtio_net_recv()` so
the frame limit is enforced without rewriting the large protocol client.

## Snapshot and reset contract

`runtime_service_get_stats()` copies one kernel-internal snapshot.
`runtime_service_reset()` clears pending readiness and measured counters while
preserving counter frequency and timing threshold.

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
- Periodic and network readiness are independent.
- Requests coalesce and backend requeue survives.
- Aggregate duration and all current class metrics are measurable.
- Post-EOI network RX consumes at most 16 valid frames per pass.
- Reaching the network cap republishes network readiness and increments an
  exhaustion counter.
- Input queue depth, high-water, and overflow are measurable.
- Reports outside the active bottom half are ignored.
- `.data == 0`, the 108000-byte kernel ceiling, and the user ABI are preserved.

## Guarantees not implemented

ArmoniOS does not yet guarantee:

- maximum total service or interrupt-to-EL0 latency;
- maximum input events consumed per pass;
- maximum USB HID polls per pass;
- maximum redraw or damage work per pass;
- a global generic-counter deadline;
- fairness among all work classes and EL0;
- no input queue overflow under sustained load;
- measured device-level network drops;
- bounded cooperative network polling outside the runtime service;
- SMP-safe publication or snapshots;
- a separately schedulable runtime thread.

The network budget progresses but does not close `RISK-017`.

## Verification

`tests/run_runtime_service_test.sh` verifies:

- EOI-before-backend order;
- request coalescing and generic requeue preservation;
- last/max/total timing and interval overruns;
- every current indexed metric;
- partial and full redraw helper behavior;
- exactly 16 network frames followed by a conservative empty check;
- 17 frames split across two passes without silent loss;
- network-budget exhaustion and pending-bit preservation;
- network polling outside the active service remaining unbudgeted;
- reset and snapshot behavior;
- static timer, network, USB, and redraw wiring.

The shorter hosted workflow runs this script with strict `pipefail` and uploads a
`runtime-service-test-log` artifact even on failure.

Validated implementation head:
`e3765864e6719c0b6373a4c9b1b7db59dfaa0202`.

- `Verify ArmoniOS` run `29849603386`: success;
- `CI - Tests` run `29849603374`: success;
- loadable QEMU kernel: 107548 / 108000 bytes.

Merged through PR #50 as
`3797f7e7cf3dfb825d927e399aa4769b27020e29`.

## Remaining Phase 2 behavior

The remaining bounded service should enforce:

```text
input consumed      <= N events/pass
USB HID polling     <= N operations/pass
redraw/damage       <= bounded batch/pass
global time         <= T generic-counter ticks
```

For every class or deadline exhaustion:

1. stop processing that work;
2. preserve or republish its specific pending bit;
3. record exhaustion;
4. return toward process dispatch;
5. continue on a later wakeup.

Values must be justified by fixed queue limits and sustained QEMU evidence.

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
