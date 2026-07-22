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

The QEMU timer runs at 100 Hz and configures one nominal timer interval as the
service-wide generic-counter deadline:

```text
deadline_ticks = CNTFRQ_EL0 / timer_hz
```

At 100 Hz this is approximately 10 ms.

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
       -> record deadline = CNTPCT_EL0 + budget_ticks
       -> PERIODIC / INPUT phase
            -> virtio-input producer: <= min(queue_size, 16) descriptors
            -> USB HID producer: <= 4 registered device visits
            -> shared input consumer: <= 16 events when INPUT is pending
            -> partial redraw: <= 8 damage rectangles/successful submission
            -> full redraw: one non-preemptible operation
            -> deadline checkpoints at metric and redraw boundaries
       -> NETWORK phase, only when deadline remains
            -> consume <= 16 valid virtio-net RX frames
            -> checkpoint after completed frames
       -> on deadline exhaustion
            -> record one over-budget event
            -> republish the original work snapshot
            -> skip later optional work
       -> update duration and requeue telemetry
  -> process_dispatch_next()
  -> eret
```

EOI completes the interrupt-controller transaction; it does not return from the
CPU exception. During the service pass execution remains in EL1, the exception
frame remains on the EL1 stack, nested IRQ helpers preserve the vector's masked
state, and EL0 remains paused.

## Deadline contract

Production timing uses the AArch64 physical generic counter:

- `CNTPCT_EL0` supplies pass start, checkpoints, and end;
- `CNTFRQ_EL0` supplies the conversion frequency;
- `budget_ticks` is one nominal timer interval.

At a safe checkpoint, when `CNTPCT_EL0 >= deadline`:

1. the active phase becomes `RUNTIME_PHASE_DEADLINE`;
2. `over_budget_count` increments once;
3. the original `last_work` snapshot is ORed back into `pending_work`;
4. later optional work classes are not started;
5. control returns toward process dispatch.

The republish rule is deliberately conservative. A completed class may be
scheduled once more, but no class from an expired pass is silently forgotten.
Native continuation remains in the input queue, virtio rings, and compositor
damage list.

The deadline is cooperative, not asynchronous preemption. It cannot interrupt a
single operation already in progress. In particular, a full redraw remains one
explicit operation and may cross the deadline before the next checkpoint. The
pass is then recorded as exhausted and its readiness is republished.

## Pending-work model

The pending word accepts:

```text
RUNTIME_WORK_PERIODIC
RUNTIME_WORK_INPUT
RUNTIME_WORK_NETWORK
```

Publication masks unknown bits and ORs accepted readiness into the canonical
`runtime_service_stats_t.pending_work` field. Repeated requests coalesce.
Consumption clears the snapshot before running the backend. Backend requeue and
deadline republish survive for the next pass.

Bits represent readiness, not exact event counts:

- `PERIODIC` owns fixed producer scans and GUI flush opportunity;
- `INPUT` permits up to 16 shared-queue pops and requeues when events remain;
- `NETWORK` permits up to 16 valid RX frames and conservatively requeues at cap.

## Enforced count budgets

| Work class | Rule | Continuation |
|---|---|---|
| Virtio-input producer | `min(queue_size, 16)` used descriptors/call | Later descriptors remain in the used ring. |
| USB HID producer | Four registered device visits/call | Every supported fixed slot fits in one scan. |
| Shared input consumer | 16 events/active input pass | Requeue when queue depth remains nonzero. |
| Partial compositor damage | Eight rectangles/successful redraw | Remaining ordered damage stays dirty. |
| Virtio-net RX | 16 valid frames/active network pass | Conservatively republish network readiness at cap. |

The network cap can schedule one empty follow-up pass when exactly 16 frames were
available. Exhaustion means the cap was reached, not proof that a seventeenth
frame existed.

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

The snapshot also records requests, coalescing, non-empty and empty passes,
requeues, last/max/total duration, global deadline exhaustion through
`over_budget_count`, input/network count-budget exhaustion, queue pressure,
counter frequency, configured budget, pending work, and last-consumed work.

The current virtio-net interface has no trustworthy counter for device-level
frames dropped, overwritten, or never delivered to software. Consumed-frame
counts are not evidence of loss-free receive.

## Routing contract

The kernel orchestrator routes existing calls through runtime wrappers:

- `input_queue_poll()` -> `runtime_service_input_poll()`;
- `gui_render()` -> `runtime_service_gui_render()`;
- `gui_clear_dirty()` -> `runtime_service_gui_clear_dirty()`;
- `net_poll()` -> `runtime_service_net_poll()`.

The wrappers preserve cooperative behavior outside the active runtime service.
During the periodic phase, the input cap, redraw batch, and deadline checkpoints
apply. The inherited periodic network call is suppressed; the independent
network phase invokes the bounded receive path only while time remains.

## Snapshot and reset contract

`runtime_service_get_stats()` copies one kernel-internal snapshot.
`runtime_service_reset()` clears pending readiness, transient phase/redraw state,
and measured counters while preserving counter frequency and configured budget.

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
- A service-wide generic-counter deadline is enforced at safe checkpoints.
- Deadline exhaustion is counted once and the original work snapshot is
  conservatively republished.
- Later optional classes are skipped after deadline exhaustion.
- Virtio-input processes at most one negotiated ring length and no more than 16
  descriptors per call.
- USB HID visits at most four device slots per call.
- Shared input consumption is at most 16 events per active pass.
- Partial redraw processes at most eight damage rectangles per successful
  submission and preserves all remaining or failed damage.
- Post-EOI network RX is at most 16 valid frames per pass.
- Reports outside the active bottom half are ignored.
- `.data == 0`, the 108000-byte kernel ceiling, and the user ABI are preserved.

## QEMU stress evidence

`tools/qemu_runtime_stress_test.sh` builds a separate instrumented image. The
production image and size gate are built without `ARMONIOS_RUNTIME_STRESS_TEST`.

The stress image:

- launches the panel auto-test so real windows and redraw submissions occur;
- boots virtio-net through DHCP and records a valid consumed frame;
- attaches xHCI keyboard and mouse devices;
- injects repeated keyboard events for 12 seconds through QEMU's monitor;
- emits an EL0 heartbeat from repeated `SYS_YIELD` calls;
- forces one test-only cooperative deadline expiry every eight service passes;
- records each expiry after the production republish path runs;
- fails on observable input queue overflow or kernel panic.

Validated head `fd2deb8e6ef6999f26a688000c37ab22a4bc46f6` produced:

```text
EL0 heartbeat markers:        509
deadline republish markers:   311
input-consumed marker:          1
redraw-submitted marker:        1
network-frame marker:           1
DHCP acknowledgements:          1
input-overflow markers:         0
panic markers:                  0
```

The serial log is uploaded in the `qemu-serial-logs` workflow artifact. Ordinary
boot strings are not used as stress assertions because IRQ diagnostics can
legally interleave character-by-character with EL0 serial writes. Assertions use
work-path markers and counted heartbeats instead.

This proves repeated EL0 execution while the actual deadline-republish path is
exercised hundreds of times, with real input consumption, GUI submission, and
network receive activity present in the same QEMU run. It also proves that the
chosen keyboard injection rate did not trigger the observable input-overflow
counter.

## Remaining evidence boundary

The stress gate intentionally forces deterministic expirations in its separate
test image. It does not establish the maximum production pass duration under the
natural 10 ms threshold.

The run observes real virtio-net receive and DHCP activity, but does not create a
sustained RX backlog or prove device-level loss behavior. The current driver does
not expose trustworthy device-drop or overwrite counters.

ArmoniOS therefore still lacks evidence for:

- sustained virtio-net RX pressure beyond boot/DHCP traffic;
- a measured production-threshold maximum under realistic combined load;
- asynchronous interruption of one already-started full redraw or driver call;
- fairness among all work classes under an indefinitely maintained backlog.

The deadline cannot preempt an operation already executing, so a full redraw may
cross the nominal interval once. Cooperative network polling outside the runtime
service also remains outside this post-EOI guarantee.

These remaining boundaries keep `RISK-017` open rather than overstating the new
stress result.

## Deterministic verification

`tests/runtime_deadline_test.c` verifies:

- a periodic checkpoint can expire before redraw and prevent the later network
  phase;
- an active network loop stops at a deadline checkpoint and retains frames;
- an operation that completes after the deadline is detected, counted, and
  conservatively republished.

The existing runtime regressions continue to verify EOI ordering, coalescing,
reset, class metrics, exact input/network caps, virtio-input 8 + 2 continuation,
USB four-slot clamping, redraw 8 + 8 + 4 continuation, failed redraw retention,
and full redraw behavior.

Validation for the stress head:

- `Verify ArmoniOS` run `29893037276` (#280): success;
- `CI - Tests` run `29893037263` (#420): success;
- production loadable QEMU kernel remains 107930 / 108000 bytes;
- remaining production margin remains 70 bytes;
- deadline implementation PR: #60;
- QEMU stress/heartbeat PR: #61.

## RISK-017 remaining exit criteria

1. generate sustained virtio-net RX pressure, not only DHCP/startup traffic;
2. expose or explicitly delimit device-level RX loss evidence;
3. record production-threshold pass durations under realistic combined load;
4. decide whether an already-started full redraw needs finer-grained checkpoints;
5. retain every existing subsystem and stress gate;
6. record a dated visible desktop pass after the final runtime boundary.

A later scheduler may promote this work into a wakeable EL1 service, but the
current bottom half first needs the remaining network and natural-duration
evidence.
