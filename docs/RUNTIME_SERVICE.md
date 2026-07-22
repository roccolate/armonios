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
only while the runtime backend is active, so unrelated cooperative work does not
contaminate bottom-half measurements.

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

Input and GUI wrappers retain their documented cooperative behavior where
needed. Network receive is stricter: `runtime_service_net_poll()` and
`runtime_service_virtio_net_recv()` consume nothing unless the active phase is
`RUNTIME_WORK_NETWORK`. The legacy console-thread poll call therefore cannot
drain the ring outside the post-EOI count and time budgets.

During the periodic phase, the input cap, redraw batch, and deadline checkpoints
apply. The inherited periodic network call is suppressed; the independent
network phase invokes the bounded receive path only while time remains.

## Snapshot and reset contract

`runtime_service_get_stats()` copies one kernel-internal snapshot using the
freestanding kernel copy primitive. `runtime_service_reset()` clears pending
readiness, transient phase/redraw state, and measured counters while preserving
counter frequency and configured budget.

All state remains zero-initialized, preserving `.data == 0`. No production
syscall exposes the internal layout. Test-only QEMU images may read it directly
inside EL1 with IRQs masked to emit coherent serial evidence. Any Monitor
integration requires a versioned diagnostic ABI.

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
- Network receive cannot bypass the active NETWORK phase.
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
- `.data == 0`, the 108000-byte production kernel ceiling, and the user ABI are
  preserved.

## Deterministic forced-expiry stress

`tools/qemu_runtime_stress_test.sh` builds a separate image with
`ARMONIOS_RUNTIME_STRESS_TEST`. It launches real windows, completes DHCP, injects
USB keyboard events, forces one checkpoint expiry every eight service passes,
and emits EL0 heartbeats.

The validated PR #61 run produced:

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

This proves repeated EL0 execution while the actual deadline-republish path is
exercised hundreds of times. Forced expiry is deterministic instrumentation, not
a measurement of natural production latency.

## Natural virtio-net RX saturation

`tools/qemu_runtime_net_stress_test.sh` builds a separate image with
`ARMONIOS_RUNTIME_NET_STRESS_TEST`; it does not shorten the budget or force the
clock. After DHCP, QEMU `hostfwd` injects sustained UDP traffic while xHCI
keyboard events and panel redraw work continue.

The final PR #62 stress run recorded:

```text
EL0 yields:                       38,912
input events consumed:                 16
redraw submissions:                   738
virtio-net frames consumed:        29,234
maximum frames in one pass:            16
network-cap exhaustions:             1,827
runtime requeues:                    1,827
natural deadline overruns:               0
maximum pass duration:             385,763 ticks
configured budget:                 625,000 ticks
maximum / budget:                    61.7%
input queue overflow:                    0
kernel panic:                            0
```

The run contains 38 coherent EL0 summaries. It demonstrates repeated progress
while the 16-frame cap is reached and requeued thousands of times, with the
natural maximum remaining below one timer interval. The instrumented image is
108438 bytes and is not the release artifact; the production image remains under
the unchanged 108000-byte gate.

## Evidence boundary

The natural saturation gate proves software-visible continuation and latency for
the tested QEMU workload. It does not prove that every host-submitted packet
reached a guest descriptor. The current driver exposes no trustworthy device or
ring-drop counter, so frames dropped or overwritten before software consumption
remain outside the evidence boundary.

The deadline also cannot preempt an operation already executing. A full redraw or
driver call may cross the nominal interval once even though the measured
saturation run did not. Indefinite fairness among all classes is not formally
proved.

An intermittent EL1 VMM data abort observed once during an existing FAT32 smoke
run is tracked separately in issue #63 and is not attributed to the runtime RX
change without reproduction evidence.

## Validation

Final implementation/evidence head:
`eac4ff990baddbf83406567b4a20e58bcae6600d`.

- `Verify ArmoniOS` run `29896102906` (#290): success;
- `CI - Tests` run `29896102904` (#430): success;
- production loadable QEMU kernel: 107918 / 108000 bytes;
- remaining production margin: 82 bytes;
- global deadline: PR #60;
- forced-expiry heartbeat: PR #61;
- natural RX saturation and strict routing: PR #62.

## RISK-017 remaining exit criteria

Automated runtime evidence is now complete for the current QEMU v0.2 contract.
Before formal promotion:

1. explicitly accept the lack of device-level RX-drop telemetry or schedule a
   separate driver counter milestone;
2. decide whether the full-redraw non-preemptible boundary is acceptable for
   v0.2;
3. retain every existing subsystem and stress gate;
4. record a dated visible desktop pass after the final runtime boundary;
5. resolve or explicitly disposition issue #63 before tagging v0.2.

A later scheduler may promote the bottom half into a wakeable EL1 service, but
that is no longer required to demonstrate the current bounded cooperative
contract.
