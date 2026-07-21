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
            -> one dirty redraw submission path
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

Input producers, input consumption, and post-EOI network RX are now count-
bounded. Redraw/damage and complete exception-to-EL0-return time are not yet
fully bounded.

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

Virtio-input producer continuation does not need a pending bit beyond periodic
readiness: unconsumed descriptors remain in the used ring and are seen on the
next timer tick. USB has no continuation cursor because every supported slot
fits in one fixed four-device scan.

## Timing telemetry

Production timing uses the AArch64 physical generic counter:

- `CNTPCT_EL0`: pass start and end;
- `CNTFRQ_EL0`: conversion frequency.

`timer_init()` configures one timer interval as the observation threshold:

```text
threshold_ticks = CNTFRQ_EL0 / timer_hz
```

At 100 Hz this is approximately 10 ms. Exceeding it means one service pass used
more than a nominal timer interval. It is not the final enforced global deadline.

The internal snapshot records requests, coalescing, non-empty/empty passes,
requeues, duration values, interval overruns, counter frequency, threshold,
pending work, last-consumed work, class metrics, and exhaustion counters.

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
only while the runtime backend is active, so cooperative console-thread work
does not contaminate bottom-half measurements.

### Input production and queue pressure

- `INPUT_PRODUCED` counts events successfully queued by virtio-input and direct
  USB HID producers.
- `INPUT_CONSUMED` counts successful shared-queue pops during the active input
  phase.
- The 64-event queue records current depth, lifetime high-water, and rejected
  full-queue pushes.

`virtio_input_poll()` now returns the number of events actually queued. It may
process no more than the negotiated ring size and never more than
`VIRTIO_INPUT_POLL_BUDGET == 16` used descriptors per call. Later descriptors
remain in the ring.

The shared consumer accepts at most `RUNTIME_INPUT_EVENT_BUDGET == 16` events per
active pass. At the cap it checks queue depth without consuming. If work remains,
`RUNTIME_WORK_INPUT` is republished and input-budget exhaustion increments.

### USB HID polling

`DEVICE_POLLS` increments for each valid HID poll that reaches the active xHCI
controller. It is separate from produced input because a poll may perform
controller work while yielding no event.

The kernel-wide HID state has four slots. `usb_hid_poll_all()` enforces:

```text
USB_HID_POLL_BUDGET == USB_HID_MAX_DEVICES == 4
```

Even a malformed `count` cannot scan beyond the array. Each visited device
receives one interrupt-in attempt. The xHCI transfer path has finite spin limits,
but only the future global deadline can bound combined producer time against a
single service-wide clock.

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
not measured. Redraw work still lacks an explicit per-pass or time stop rule.

## Enforced count budgets

### Network RX

```text
RUNTIME_NETWORK_FRAME_BUDGET == 16
```

During an active network phase, valid frames are consumed until 16 have
completed. The next receive attempt returns no frame, increments network-budget
exhaustion once, republishes `RUNTIME_WORK_NETWORK`, and returns toward process
dispatch.

The requeue is deliberately conservative. Exactly 16 frames may schedule one
empty follow-up pass. Exhaustion means the cap was reached, not proof that a
seventeenth frame existed.

### Shared input consumption

```text
RUNTIME_INPUT_EVENT_BUDGET == 16
```

At most 16 shared-queue events are consumed. Unlike network RX, input can query
queue depth cheaply and reliably:

- exactly 16 events followed by an empty queue do not requeue;
- a seventeenth queued event is consumed on a later pass;
- exhaustion means the cap was reached while queue work remained.

### Virtio-input producer

```text
VIRTIO_INPUT_POLL_BUDGET == 16
```

One call processes at most `min(device->queue_size, 16)` used descriptors.
Descriptor recycling and queue notification remain unchanged. A deterministic
host test negotiates an eight-entry ring, injects ten used descriptors, and
proves completion as 8 + 2 across two calls with ten queued events.

### USB HID producer

```text
USB_HID_POLL_BUDGET == 4
```

One call visits at most the four fixed HID slots. A deterministic host test sets
the public count field to 255 and proves only four iterations occur.

## Routing contract

The kernel orchestrator routes shared input consumption through
`runtime_service_input_poll()`.

- During the active periodic phase without `INPUT`, consumption is suppressed.
- During an active input phase, the 16-event consumer budget applies.
- Outside the runtime service, cooperative callers pass through unbudgeted.

The existing periodic backend calls board input, USB HID, GUI input consumption,
and redraw once. Its inherited network call is suppressed. The independent
network phase uses `runtime_service_net_poll()` and the DHCP receive path is
compiled through `runtime_service_virtio_net_recv()`.

## Snapshot and reset contract

`runtime_service_get_stats()` copies one kernel-internal snapshot.
`runtime_service_reset()` clears pending readiness, phase state, and measured
counters while preserving counter frequency and timing threshold.

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
- Aggregate duration and all current class metrics are measurable.
- Virtio-input processes at most one negotiated ring length and no more than 16
  descriptors per call.
- USB HID visits at most four device slots per call.
- Shared input consumption is at most 16 events per active pass.
- Post-EOI network RX is at most 16 valid frames per pass.
- Input and network continuation preserve work without silent discard.
- Reports outside the active bottom half are ignored.
- `.data == 0`, the 108000-byte kernel ceiling, and the user ABI are preserved.

## Guarantees not implemented

ArmoniOS does not yet guarantee:

- maximum total service or interrupt-to-EL0 latency;
- maximum redraw or damage CPU/time work;
- a global generic-counter deadline;
- fairness among all work classes and EL0 under sustained combined pressure;
- no input queue overflow under sustained load;
- measured device-level network drops;
- bounded cooperative network polling outside the runtime service;
- SMP-safe publication or snapshots;
- a separately schedulable runtime thread.

The producer and consumer bounds progress but do not close `RISK-017`.

## Verification

The hosted and local gates verify:

- EOI-before-backend order;
- request coalescing and generic requeue preservation;
- last/max/total timing and interval overruns;
- every indexed class metric;
- exact network and input consumer caps and continuation;
- virtio-input negotiated-ring 8 + 2 continuation;
- malformed USB count clamped to four visits;
- reset, outside-service behavior, and static timer/network/USB/redraw wiring;
- full QEMU/RPi4, storage, GUI, stack, ABI, and subsystem baseline.

Latest validated implementation head:
`ee92e8074ed2995a48ce22fb88a901ea02cf031d`.

- `Verify ArmoniOS` run `29859659229`: success;
- `CI - Tests` run `29859659270`: success;
- loadable QEMU kernel: 107706 / 108000 bytes.

Producer bounds merged through:

- PR #55: `53c1440261267b36e813fb90e6405261ec7bbfad`;
- PR #56: `7674b639b9a53dea4cec42bcccf84e71d7f6d10c`.

## Remaining Phase 2 behavior

The remaining bounded service should enforce:

```text
redraw/damage <= explicit bounded batch or time slice
service time  <= T generic-counter ticks
```

For every class or deadline exhaustion:

1. stop processing that work;
2. preserve or republish its specific readiness;
3. record exhaustion;
4. return toward process dispatch;
5. continue on a later wakeup.

Values must be justified by fixed limits and sustained QEMU evidence.

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
