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
       -> publish PERIODIC | INPUT | NETWORK readiness
       -> update scheduler counters
  -> board_irq_end()
  -> runtime_service_run_pending()
       -> snapshot and clear pending readiness
       -> read CNTPCT_EL0
       -> mark active service pass
       -> PERIODIC / INPUT phase
            -> virtio-input producer: <= min(queue_size, 16) descriptors
            -> USB HID producer: <= 4 registered device visits
            -> shared input consumer: <= 16 events when INPUT is pending
            -> partial redraw: <= 8 damage rectangles/successful submission
            -> full redraw: one operation
            -> inherited network call suppressed
       -> NETWORK phase
            -> consume <= 16 valid virtio-net RX frames
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

Input producers, input consumption, partial compositor damage, and post-EOI
network RX are count-bounded. A full redraw and complete exception-to-EL0-return
time are not yet globally time-bounded.

## Pending-work model

The pending word accepts:

```text
RUNTIME_WORK_PERIODIC
RUNTIME_WORK_INPUT
RUNTIME_WORK_NETWORK
```

Publication masks unknown bits and ORs accepted readiness into the canonical
`runtime_service_stats_t.pending_work` field. Repeated requests coalesce.
Consumption clears the snapshot before running the backend:

```c
snapshot = pending;
pending = 0;
run(snapshot);
```

A request republished by the backend survives for the next pass. Bits represent
readiness, not exact timer or device-event counts.

- `PERIODIC` owns fixed producer scans and GUI flush opportunity.
- `INPUT` permits up to 16 shared-queue pops and requeues only if events remain.
- `NETWORK` permits up to 16 valid RX frames and conservatively requeues at cap.

Virtio-input continuation remains in the used ring. USB needs no cursor because
all four supported slots fit in one scan. Partial redraw continuation remains in
the compositor's existing damage list; `dirty` stays set until all successful
batches complete.

## Timing telemetry

Production timing uses the AArch64 physical generic counter:

- `CNTPCT_EL0`: pass start and end;
- `CNTFRQ_EL0`: conversion frequency.

`timer_init()` configures one timer interval as the observation threshold:

```text
threshold_ticks = CNTFRQ_EL0 / timer_hz
```

At 100 Hz this is approximately 10 ms. Exceeding it means one service pass used
more than a nominal timer interval. It is not an enforced deadline and does not
stop work mid-pass.

The internal snapshot records requests, coalescing, non-empty/empty passes,
requeues, duration values, interval overruns, counter frequency, threshold,
pending work, last-consumed work, class metrics, and input/network exhaustion
counters.

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
RUNTIME_METRIC_REDRAW_EXHAUSTIONS
```

Each stores last-pass, maximum-pass, and cumulative counts. Reports are accepted
only while the runtime backend is active, so cooperative console-thread work
does not contaminate bottom-half measurements.

### Input production and queue pressure

- `INPUT_PRODUCED` counts events successfully queued by virtio-input and direct
  USB HID producers.
- `INPUT_CONSUMED` counts successful shared-queue pops during the active input
  phase.
- The 64-event queue records current depth, lifetime high-water, and rejected
  full-queue pushes.

`virtio_input_poll()` returns the number of events actually queued. It processes
no more than the negotiated ring size and never more than
`VIRTIO_INPUT_POLL_BUDGET == 16` used descriptors per call. Later descriptors
remain in the ring.

The shared consumer accepts at most `RUNTIME_INPUT_EVENT_BUDGET == 16` events per
active pass. At the cap it checks queue depth without consuming. If work remains,
`RUNTIME_WORK_INPUT` is republished and input-budget exhaustion increments.

### USB HID polling

`DEVICE_POLLS` increments for each valid HID poll that reaches the active xHCI
controller. It is separate from produced input because a poll may perform
controller work while yielding no event.

The kernel-wide HID state has four slots:

```text
USB_HID_POLL_BUDGET == USB_HID_MAX_DEVICES == 4
```

Even a malformed count cannot scan beyond the array. Each visited device receives
one interrupt-in attempt. The xHCI path has finite spin limits, but only the
future global deadline can bound combined elapsed time against a service-wide
clock.

### Network frames

`NETWORK_FRAMES` increments for each valid non-empty virtio-net RX frame returned
during the active network phase. It counts completed software consumption, not
calls to `net_poll()`.

The current virtio-net path has 16 RX descriptors and no trustworthy device-drop,
overwrite, or ring-overflow counter. Consumed-frame counts must not be presented
as proof of loss-free receive.

### Redraw and damage

- `REDRAW` counts successful board submissions.
- `DAMAGE_ITEMS` counts partial-damage rectangles in successful batches.
- `FULL_REDRAWS` counts successful submissions using the full-redraw sentinel.
- `REDRAW_EXHAUSTIONS` counts successful partial redraws that leave damage for a
  later periodic pass.

Partial redraw uses:

```text
RUNTIME_REDRAW_DAMAGE_BUDGET == 8
```

The runtime wrapper temporarily exposes only the first eight rectangles to
`gui_render()`, then restores the original list before submission success is
known. After a successful board submission, the consumed prefix is removed and
the remainder stays in-order. After a failed submission, no damage is removed
and `dirty` remains set.

A full redraw is one explicit operation and clears on success. Its pixel count,
CPU cost, and GPU completion time are not bounded by the count rule; the future
global deadline must cover elapsed service time.

## Enforced count budgets

| Work class | Rule | Continuation |
|---|---|---|
| Virtio-input producer | `min(queue_size, 16)` used descriptors/call | Later descriptors remain in the used ring. |
| USB HID producer | Four registered device visits/call | Every supported fixed slot fits in one scan. |
| Shared input consumer | 16 events/active input pass | Requeue only when queue depth remains nonzero. |
| Partial compositor damage | Eight rectangles/successful redraw | Remaining ordered damage stays dirty for later periodic passes. |
| Virtio-net RX | 16 valid frames/active network pass | Conservatively republish network readiness at cap. |

The network requeue is deliberately conservative. Exactly 16 frames may
schedule one empty follow-up pass. Exhaustion means the cap was reached, not
proof that a seventeenth frame existed.

## Routing contract

The kernel orchestrator routes existing calls through runtime wrappers:

- `input_queue_poll()` -> `runtime_service_input_poll()`;
- `gui_render()` -> `runtime_service_gui_render()`;
- `gui_clear_dirty()` -> `runtime_service_gui_clear_dirty()`;
- `net_poll()` -> `runtime_service_net_poll()`.

The wrappers preserve cooperative behavior outside the active runtime service.
During the periodic phase, the input consumer cap and redraw batch rules apply.
The inherited network call is suppressed; the independent network phase invokes
the bounded receive path.

## Snapshot and reset contract

`runtime_service_get_stats()` copies one kernel-internal snapshot.
`runtime_service_reset()` clears pending readiness, phase/redraw transient state,
and measured counters while preserving counter frequency and observation
threshold.

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
- Periodic, input, and network readiness follow explicit routing rules.
- Requests coalesce and backend requeue survives.
- Aggregate duration and current class metrics are measurable.
- Virtio-input processes at most one negotiated ring length and no more than 16
  descriptors per call.
- USB HID visits at most four device slots per call.
- Shared input consumption is at most 16 events per active pass.
- Partial redraw processes at most eight damage rectangles per successful
  submission and preserves all remaining or failed damage.
- Post-EOI network RX is at most 16 valid frames per pass.
- Reports outside the active bottom half are ignored.
- `.data == 0`, the 108000-byte kernel ceiling, and the user ABI are preserved.

## Guarantees not implemented

ArmoniOS does not yet guarantee:

- maximum total service or interrupt-to-EL0 latency;
- an elapsed-time bound for a full redraw;
- an enforced global generic-counter deadline;
- fairness among all work classes and EL0 under sustained combined pressure;
- no input queue overflow under sustained load;
- measured device-level network drops;
- bounded cooperative network polling outside the runtime service;
- SMP-safe publication or snapshots;
- a separately schedulable runtime thread.

The count budgets progress but do not close `RISK-017`.

## Verification

The hosted and local gates verify:

- EOI-before-backend order;
- request coalescing and generic requeue preservation;
- last/max/total timing and interval overruns;
- every indexed class metric;
- exact network and input consumer caps and continuation;
- virtio-input negotiated-ring 8 + 2 continuation;
- malformed USB count clamped to four visits;
- partial redraw completion as 8 + 8 + 4;
- failed redraw preserving five rectangles;
- full-redraw and reset behavior;
- full QEMU/RPi4, storage, GUI, stack, ABI, and subsystem baseline.

Latest validated implementation head:
`8b86a8c24f25af0937f1df2e983c1c7c4f489b7d`.

- `Verify ArmoniOS` run `29863653280`: success;
- `CI - Tests` run `29863653209`: success;
- loadable QEMU kernel: 107982 / 108000 bytes;
- PR #58 merge: `fe4f2a622f5633e55b0eddb2f8f6767453a9ddca`.

## Remaining Phase 2 behavior

The remaining production work is:

```text
service elapsed time <= T generic-counter ticks
```

Before adding that code, another compacting change is required because the
current loadable kernel has only 18 bytes of margin.

A correct deadline implementation must:

1. check elapsed time at class boundaries and safe inner-loop boundaries;
2. stop before starting optional work once the deadline is exhausted;
3. preserve or republish unfinished class readiness;
4. retain work in native queues/rings/damage lists where applicable;
5. record deadline exhaustion;
6. return toward process dispatch.

The selected threshold must be justified by fixed limits and sustained QEMU
evidence, not by the current observation threshold alone.

## RISK-017 exit criteria

The risk remains open until deterministic and sustained-load QEMU tests prove:

- EL0 heartbeat progress under input, USB, network, and redraw pressure;
- combined work cannot indefinitely delay dispatch;
- every exhausted class remains pending or retains work in its native queue;
- observable queue loss is absent or explicitly counted;
- maximum complete-pass duration stays below a documented enforced threshold;
- all existing subsystem gates remain green.

A later scheduler may promote this work into a wakeable EL1 service, but the
current bottom half must first become completely bounded and non-blocking.
