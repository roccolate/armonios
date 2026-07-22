# Deferred Runtime Service

## Purpose

ArmoniOS keeps the physical timer callback independent of GUI, device, and
network workload. The timer publishes readiness and one centralized service
consumes it after interrupt-controller EOI.

The exact boundary is:

> deferred past the hard timer callback and EOI, but still inside the IRQ
> exception path before process dispatch and `eret`.

It is a post-EOI EL1 bottom half. It is not a thread, not independently
preemptible, and not SMP-safe.

Operational evidence lives in `CURRENT_STATE.md`; residuals live in
`TECHNICAL_RISKS.md`; change rules live in `DEVELOPMENT_GUIDE.md`.

## Execution flow

QEMU runs the physical timer at 100 Hz and configures one nominal timer interval
as the service-wide deadline:

```text
budget_ticks = CNTFRQ_EL0 / timer_hz
deadline     = start_CNTPCT_EL0 + budget_ticks
```

At the validated QEMU counter frequency, the recorded test budget is 625000 ticks,
approximately 10 ms.

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
       -> record deadline
       -> PERIODIC / INPUT phase
            -> virtio-input producer
            -> USB HID producer
            -> shared input consumer
            -> GUI redraw opportunity
       -> NETWORK phase, if time remains
            -> bounded virtio-net RX
       -> on expiry
            -> count once
            -> republish original work snapshot
            -> skip later optional work
       -> update duration and requeue telemetry
  -> process_dispatch_next()
  -> eret
```

EOI completes the interrupt-controller transaction. It does not return from the
CPU exception. During the service pass:

- execution remains in EL1;
- the exception frame remains on the EL1 stack;
- normal IRQs remain masked by vector-entry state;
- nested IRQ helpers preserve that prior mask state;
- EL0 remains paused.

## Pending-work model

The canonical pending word accepts:

```text
RUNTIME_WORK_PERIODIC
RUNTIME_WORK_INPUT
RUNTIME_WORK_NETWORK
```

Unknown bits are masked. Repeated requests coalesce through OR.

The service snapshots and clears the pending word before running. Requests
published during the current pass therefore remain pending for the next pass.
Backend budget requeues and deadline republish also survive.

Readiness bits are not exact event counts:

- `PERIODIC` owns fixed producer scans and one GUI flush opportunity;
- `INPUT` permits bounded shared-queue consumption;
- `NETWORK` permits bounded network receive.

Exact continuation remains in native structures such as device rings, the shared
input queue, and the compositor damage list.

## Service-wide deadline

Production timing uses the AArch64 physical generic counter:

- `CNTPCT_EL0` supplies start, checkpoints, and end;
- `CNTFRQ_EL0` supplies conversion frequency;
- `budget_ticks` is one nominal timer interval.

At a safe checkpoint, when `CNTPCT_EL0 >= deadline`:

1. the active phase becomes `RUNTIME_PHASE_DEADLINE`;
2. `over_budget_count` increments once;
3. the original `last_work` snapshot is ORed into `pending_work`;
4. later optional work classes are not started;
5. control returns toward process dispatch.

The republish rule is deliberately conservative. A class that completed may run
again, but no readiness class from an expired pass is silently forgotten.

The deadline is cooperative, not asynchronous preemption. It cannot interrupt one
operation already executing. A full redraw or driver call may cross the nominal
interval before the next checkpoint; the pass is then counted and republished.

## Enforced count budgets

| Work class | Rule | Native continuation |
|---|---|---|
| Virtio-input producer | At most one negotiated ring length and no more than 16 used descriptors/call | Later descriptors remain in the used ring. |
| USB HID producer | Four registered-device visits/call | All supported direct slots fit in one scan. |
| Shared input consumer | 16 events/active INPUT pass | INPUT requeued when events remain. |
| Partial compositor damage | Eight ordered rectangles/successful submission | Remaining damage stays dirty; failure consumes none. |
| Virtio-net RX | 16 valid frames/active NETWORK pass | NETWORK conservatively requeued at cap. |

The network cap may schedule one empty follow-up pass when exactly 16 frames were
available. Exhaustion means the cap was reached, not proof that a seventeenth
frame existed.

## Routing contract

The kernel orchestrator routes existing calls through wrappers:

- `input_queue_poll()` -> `runtime_service_input_poll()`;
- `gui_render()` -> `runtime_service_gui_render()`;
- `gui_clear_dirty()` -> `runtime_service_gui_clear_dirty()`;
- `net_poll()` -> `runtime_service_net_poll()`;
- virtio-net descriptor receive -> `runtime_service_virtio_net_recv()`.

Input and GUI wrappers preserve their documented behavior inside and outside the
service where required.

Network routing is stricter:

- `runtime_service_net_poll()` consumes nothing unless the active phase includes
  `RUNTIME_WORK_NETWORK`;
- `runtime_service_virtio_net_recv()` returns no frame outside NETWORK phase;
- inherited periodic/cooperative network calls cannot drain RX outside the count
  and deadline budgets.

## Redraw contract

Partial redraw preparation temporarily exposes at most eight ordered damage
rectangles to the board renderer.

After a successful submission:

- the submitted prefix is removed;
- remaining rectangles shift left in order;
- dirty state remains while work exists.

After a failed submission:

- no damage is consumed;
- the full original list remains pending.

A full-redraw sentinel remains one explicit operation. It is not split into
smaller preemptible tiles by the current contract.

## Telemetry

Indexed metrics are:

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

Each stores last-pass, maximum-pass, and cumulative counts.

The snapshot also records:

- requests and coalescing;
- non-empty and empty passes;
- requeues;
- last, maximum, and cumulative duration;
- global deadline exhaustion;
- input and network count-budget exhaustion;
- input queue depth, lifetime high-water, and overflow;
- counter frequency and configured budget;
- pending work and last-consumed work.

Reports are accepted only while the service is active so unrelated cooperative
work does not contaminate bottom-half measurements.

The current virtio-net interface has no trustworthy counter for device/ring frames
dropped, overwritten, or never delivered to software. Consumed-frame counts are
not loss-free-delivery evidence.

## Snapshot and reset

`runtime_service_get_stats()` copies one kernel-internal snapshot using the
freestanding copy primitive. Test-only QEMU images mask IRQs while taking a serial
summary so one line reflects a coherent sample.

`runtime_service_reset()` clears measured and transient state while preserving
counter frequency and configured budget.

All mutable state remains zero-initialized, preserving production `.data == 0`.
No production syscall exposes the internal structure. Monitor integration requires
a deliberately versioned diagnostic ABI.

## Correctness assumptions

The implementation assumes:

- one CPU core;
- one runtime-service consumer;
- state-preserving nested IRQ helpers;
- no concurrent SMP writer;
- coalesced readiness rather than exact publication counts.

The pending word and telemetry are not SMP-safe atomic structures. SMP would
require per-CPU state, atomics, or a scheduler-owned queue.

## Guarantees implemented

- The physical timer callback contains no GUI, USB, input-drain, or network
  backend work.
- Runtime work begins only after `board_irq_end()`.
- PERIODIC, INPUT, and NETWORK readiness remain independent.
- Requests coalesce and requeues survive snapshot clearing.
- Network polling and receive cannot bypass NETWORK phase.
- A service-wide deadline is enforced at safe checkpoints.
- Expiry is counted once and republishes the original snapshot.
- Later optional work is skipped after expiry.
- Producer, consumer, partial-redraw, and RX count budgets are enforced.
- Native continuation is preserved.
- Reports outside the active pass are ignored.
- Production `.data == 0`, user ABI, and the 108000-byte ceiling are preserved.

## Deterministic forced-expiry stress

`tools/qemu_runtime_stress_test.sh` builds a separate image with
`ARMONIOS_RUNTIME_STRESS_TEST`. It launches real windows, completes DHCP, injects
USB keyboard events, forces one expiry every eight service passes, and emits EL0
heartbeats.

Recorded PR #61 evidence:

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

This proves liveness while the real republish path is exercised. Forced expiry is
instrumentation, not a natural production-duration measurement.

## Natural virtio-net RX saturation

`tools/qemu_runtime_net_stress_test.sh` builds a separate image with
`ARMONIOS_RUNTIME_NET_STRESS_TEST`. It does not shorten the deadline or force the
clock. After DHCP, QEMU `hostfwd` injects sustained UDP while xHCI keyboard events
and panel redraw work continue.

Recorded PR #62 evidence:

```text
EL0 yields:                       38,912
input events consumed:                 16
redraw submissions:                   738
virtio-net frames consumed:        29,234
maximum frames/pass:                   16
network cap exhaustions:             1,827
runtime requeues:                    1,827
natural deadline overruns:               0
maximum pass duration:             385,763 ticks
configured budget:                 625,000 ticks
maximum / budget:                    61.7%
input queue overflow:                    0
panic markers:                           0
```

The run emitted 38 coherent EL0 summaries. It demonstrates repeated progress while
the RX cap is reached and requeued thousands of times. The instrumented image is
not the release artifact.

## Evidence boundary

The natural stress gate proves software-visible continuation and latency for the
tested QEMU workload. It does not prove delivery of every host-submitted packet,
as device/ring drop telemetry is unavailable.

It does not prove asynchronous preemption, indefinite fairness, SMP behavior, or a
bound for every possible full-redraw/driver operation.

The intermittent FAT32/VMM panic tracked by issue #63 is a separate correctness
investigation and is not attributed to the runtime RX change without reproduction
evidence.

## Validation identity

Evidence provenance:

- original GitHub PR #62 merge metadata:
  `7ea3d309047659c8bbe9c601c3d98217bcaafb02`;
- current-main runtime replay commit:
  `d5c104a0badc3a2d553516159b2b745737dd252f`;
- implementation/evidence head:
  `eac4ff990baddbf83406567b4a20e58bcae6600d`;
- final PR #62 head:
  `04f65776d1bbe07545113652342c32f2448bfc7b`;
- original final PR validation:
  - `Verify ArmoniOS` `29896952424` (#295): success;
  - `CI - Tests` `29896952435` (#435): success;
- production kernel: 107918 / 108000 bytes;
- remaining margin: 82 bytes.

`CURRENT_STATE.md` records the current audited `main` tree and the final workflow
runs for the current documentation/promotion tree.

## Remaining v0.2 disposition

Automated evidence is complete for the current cooperative QEMU contract. Formal
promotion still requires:

1. accept or replace the missing device-level RX-drop telemetry;
2. accept or refine the one-operation full-redraw boundary;
3. retain all subsystem and stress gates;
4. record a dated visible desktop pass on the final promotion tree;
5. resolve or explicitly accept issue #63 / `RISK-018`;
6. record the exact promotion tree, runs, tag, and release limitations.
