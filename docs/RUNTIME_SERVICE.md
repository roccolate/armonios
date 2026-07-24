# Deferred runtime service

ArmoniOS keeps timer interrupt work independent from GUI, input, USB, and network
load. The physical timer callback publishes readiness; one centralized service
consumes it after interrupt-controller EOI.

The exact boundary is:

> deferred past the fixed timer callback and EOI, but still inside the IRQ
> exception path before process dispatch and `eret`.

This is a post-EOI EL1 bottom half. It is not a thread, not asynchronously
preemptible, and not SMP-safe.

Historical stress evidence is preserved in
`history/V02_RUNTIME_EVIDENCE.md`. Current operational classification lives in
`CURRENT_STATE.md`.

## IRQ-origin safety

Only an IRQ frame that interrupted EL0 may be used as schedulable process state.

```text
IRQ from EL0
  -> service devices and runtime work
  -> save/dispatch process state when required

IRQ from EL1
  -> service devices and runtime work
  -> do not save user state
  -> do not preempt through the interrupted kernel frame
  -> return to the exact EL1 path
```

The vector-side origin gate prevents EL1 register state from being mistaken for
an EL0 process frame and prevents a TTBR0/process switch while returning to an
interrupted syscall or kernel path.

## Execution flow

```text
EL0 process or EL1 kernel path
  -> physical timer IRQ
  -> timer callback
       -> account tick
       -> rearm physical timer
       -> publish readiness
       -> update scheduler counters
  -> interrupt-controller EOI
  -> runtime service
       -> snapshot and clear pending readiness
       -> establish one service-wide deadline
       -> run PERIODIC / INPUT work
       -> run NETWORK work only when requested and time remains
       -> republish conservative readiness on expiry
       -> update telemetry
  -> process dispatch only when the IRQ originated from EL0
  -> eret
```

EOI ends the interrupt-controller transaction; it does not return from the CPU
exception. During the service pass:

- execution remains in EL1;
- the exception frame remains on the EL1 stack;
- normal IRQ masking follows the vector-entry state;
- EL0 remains paused;
- one already-started operation cannot be interrupted by the cooperative
  deadline.

## Pending work

The pending word contains independent readiness classes:

```text
RUNTIME_WORK_PERIODIC
RUNTIME_WORK_INPUT
RUNTIME_WORK_NETWORK
```

Repeated requests coalesce with bitwise OR. Unknown bits are masked.

The service snapshots and clears the pending word before running. New requests
published during the pass remain pending for the next pass. Backend continuation
and deadline republication also survive the snapshot boundary.

Readiness is not an exact event count. Exact continuation stays in native data
structures:

- virtio used rings;
- the shared input queue;
- USB device slots;
- compositor damage state;
- virtio-net receive state.

## Service-wide deadline

Production timing uses the AArch64 physical generic counter:

```text
budget_ticks = CNTFRQ_EL0 / timer_hz
deadline     = start + budget_ticks
```

The budget is one nominal timer interval. Safe checkpoints compare the current
counter against the deadline.

When the deadline expires:

1. the pass records one global exhaustion;
2. the original readiness snapshot is ORed back into pending work;
3. later optional work is not started;
4. control returns toward process dispatch or the interrupted EL1 path.

The conservative republication may repeat a class that already completed, but it
prevents an expired pass from silently forgetting readiness.

The deadline is cooperative. It does not split or interrupt a driver operation or
full redraw already in progress. Such an operation may cross the nominal interval
before the next checkpoint.

## Count budgets

| Work class | Per-call/pass rule | Continuation |
|---|---:|---|
| Virtio-input producer | one negotiated ring length, capped at 16 descriptors | remaining descriptors stay in the ring |
| USB HID producer | four registered-device visits | all supported direct slots fit in one scan |
| Shared input consumer | 16 queued events | INPUT is requeued when events remain |
| Partial compositor damage | eight ordered rectangles after successful submission | remaining damage stays dirty and ordered |
| Virtio-net RX | 16 valid frames in NETWORK phase | NETWORK is conservatively requeued at the cap |

A failed GPU submission consumes no damage. Reaching the network cap records
exhaustion but does not prove a seventeenth frame existed; one empty follow-up
pass is permitted.

## Routing rules

PERIODIC owns fixed producer scans and a GUI redraw opportunity. INPUT permits
bounded consumption of the shared input queue. NETWORK permits bounded network
polling and descriptor receive.

Network routing is strict:

- network polling consumes nothing unless NETWORK is active;
- virtio-net descriptor receive returns no frame outside NETWORK phase;
- inherited cooperative calls cannot bypass the network count and deadline
  contracts.

Input and GUI wrappers preserve their documented behavior when called from
supported non-service paths, but telemetry accepts reports only while the service
is active.

## Redraw rules

Partial redraw exposes at most eight ordered rectangles to the renderer.

After successful submission:

- the submitted prefix is removed;
- remaining rectangles preserve order;
- dirty state remains while work exists.

After failed submission:

- no rectangle is consumed;
- original damage and dirty state remain pending.

A full redraw remains one explicit operation. It is not currently divided into
preemptible tiles.

## Telemetry

The internal snapshot records:

- requests and coalescing;
- empty and non-empty passes;
- requeues;
- last, maximum, and cumulative duration;
- deadline exhaustion;
- input and network budget exhaustion;
- input production, consumption, depth, high-water, and overflow;
- USB/device polling;
- redraw submissions, damage items, full redraws, and redraw exhaustion;
- software-visible network frames;
- configured counter frequency and deadline budget;
- current pending and last-consumed work.

Metrics store last-pass, maximum-pass, and cumulative values where applicable.
Reports outside an active pass are ignored so unrelated cooperative work does not
contaminate bottom-half measurements.

The current virtio-net interface has no trustworthy device/ring drop counter.
Software-consumed frames demonstrate progress, not loss-free delivery.

No production syscall exposes the internal telemetry structure. A future Monitor
integration requires a deliberately versioned diagnostic ABI.

## Reset and mutable-state contract

`runtime_service_reset()` clears measured and transient state while preserving
configuration such as counter frequency and budget.

All mutable production state is zero-initialized. The runtime service preserves
the empty loadable `.data` contract and the repository's current 128 KiB image
budget.

## Implemented guarantees

- the timer callback performs no GUI, input-drain, USB backend, or network backend
  work;
- runtime work begins only after EOI;
- IRQs from EL1 cannot enter process save/preemption;
- readiness classes remain independent and coalesced;
- requeues survive snapshot clearing;
- NETWORK work cannot be consumed outside NETWORK phase;
- the service-wide deadline is checked at safe boundaries;
- expiry is counted once and republishes conservative readiness;
- later optional work is skipped after expiry;
- producer, consumer, redraw, and network count budgets are enforced;
- native continuation is preserved;
- failed redraw consumes no damage;
- current host and QEMU stress gates exercise continuation and EL0 progress.

## Explicit non-guarantees

The current contract does not prove or provide:

- asynchronous preemption of one operation already in progress;
- a hard upper bound for every driver call or full redraw;
- trustworthy virtio device/ring drop accounting;
- indefinite fairness for every workload;
- SMP safety;
- per-CPU queues or atomic multi-consumer state;
- a schedulable wakeable runtime thread;
- a stable public telemetry ABI.

## Change discipline

A runtime-service change must preserve or deliberately revise together:

- IRQ-origin classification;
- hard-callback boundedness;
- EOI ordering;
- readiness ownership and continuation;
- count budgets;
- deadline checkpoints and republication;
- telemetry semantics;
- host regression tests;
- forced-expiry and natural-load QEMU stress coverage;
- size, stack, `.data`, ABI, and board-contract gates;
- this document and any affected risk/current-state claims.

A future wakeable EL1 service is an architectural redesign, not a documentation
rename for the current bottom half.
