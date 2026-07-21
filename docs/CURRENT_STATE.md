# Current State

> Operational source of truth for ArmoniOS.
>
> Evidence terminology: `DOCUMENTATION_POLICY.md`  
> Active risks: `TECHNICAL_RISKS.md`  
> Future milestones: `ROADMAP.md`

## Executive classification

ArmoniOS is a **v0.1 QEMU desktop baseline** and an active **v0.2
cleanup/runtime-hardening candidate**.

The QEMU `virt` kernel, graphical desktop, narrow writable FAT32 workflow,
freestanding EL0 applications, and automated verification matrix are real and
reproducible. Most original v0.2 cleanup goals have landed.

Runtime measurement Phase 1B is complete for all budget-relevant work currently
observable on QEMU. Phase 2 has begun: post-EOI network receive is now
independently pending and capped at 16 valid frames per runtime-service pass.

The service is not fully bounded. Input consumption, USB polling, redraw/damage
work, and total generic-counter time remain unlimited per pass. No sustained-load
QEMU test proves EL0 progress under combined pressure.

ArmoniOS is accurately described as:

> A compact AArch64 QEMU desktop alpha with freestanding EL0 applications, a
> kernel compositor, a narrow writable FAT32 workflow, broad automated regression
> coverage, and an initial bounded runtime work class.

It is not a production OS, general FAT implementation, POSIX system, or verified
Raspberry Pi operating system.

## Audit metadata

- **Audit date:** 2026-07-21
- **Primary verified platform:** QEMU `virt`, Cortex-A72 CPU model
- **Tracking issue:** #43, v0.2 measure and bound deferred runtime service
- **Phase 1B documentation merge:**
  `7639176ceac95abe2f44fe8eecf30d33a67478d8`
- **Network budget implementation merge:**
  `3797f7e7cf3dfb825d927e399aa4769b27020e29`
- **Validated network-budget head:**
  `e3765864e6719c0b6373a4c9b1b7db59dfaa0202`
- **Hosted validation:**
  - `Verify ArmoniOS` run `29849603386`: success
  - `CI - Tests` run `29849603374`: success
- **Loadable QEMU kernel size:** 107548 bytes
- **Kernel size limit:** 108000 bytes
- **Remaining margin:** 452 bytes

Those runs validate the implementation, deterministic network-budget regression,
strict runtime-service short-workflow gate, QEMU behavior, and all pre-existing
subsystem gates. The merge commit is not a separately executed workflow unless a
run targets that exact SHA.

## Release phases

| Phase | State | Real interpretation |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, deterministic QEMU gates, CI, and dated manual evidence exist. |
| v0.2 cleanup/hardening | IN PROGRESS / CANDIDATE | Observable work is measured and post-EOI network RX is bounded. Input, USB, redraw, global time, and stress proof remain. |
| v0.3 storage/VFS platform | NEXT AFTER v0.2 | No common path resolver, rich block metadata, or structured filesystem ABI. |
| v0.4 real FAT | PLANNED | Current FAT remains root-only 8.3 FAT32. |
| v0.5 userland runtime/widgets | PLANNED | No reusable heap, dynamic containers, or shared widget toolkit. |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Seven apps run, but issue #2's daily-use workflows are incomplete. |
| v0.7 ext2 | PLANNED | No ext2 implementation. |
| v0.8 polish | EARLY PARTIAL | Basic window/panel behavior exists; sustained visible-session evidence does not. |
| v0.9 beta | NOT STARTED | No ABI freeze, fuzz campaign, reboot-persistence gate, or beta record. |
| v1.0 | NOT READY | Storage, apps, ext2, persistence, remaining runtime bounds, and final evidence are incomplete. |

## Verification record

| Check | Evidence class | Result and scope |
|---|---|---|
| QEMU build and size | BUILD-VERIFIED | `.data == 0`; loadable kernel 107548 bytes under the 108000-byte ceiling. |
| RPi4 build/probe gates | BUILD/HOST-VERIFIED | Normal and diagnostic images build; unsupported normal capabilities fail closed. |
| Native host suite | HOST-VERIFIED | Kernel, memory, VFS, FAT32, GUI, parser, driver, and ABI tests pass. |
| Runtime service regression | HOST-VERIFIED | Timing, EOI order, coalescing, reset, all class metrics, 16-frame cap, conservative follow-up, 17-frame continuation, and outside-service network behavior pass. |
| Runtime short-workflow gate | CI-VERIFIED | Runs with strict `pipefail`; diagnostic output is retained as `runtime-service-test-log`. |
| Input queue telemetry | HOST-VERIFIED | Zero state, 64-entry high-water, full-queue overflow, draining, and reset. |
| Process/VFS/user-copy/KLI1 | HOST-VERIFIED | Parent/wait, local FDs, permission-aware copy, and mutable-storage contracts. |
| Stack check | HOST-VERIFIED | Editor maximum remains 368 bytes against 3072. |
| FAT32 smoke | QEMU-VERIFIED | Storage initialization and FAT application markers. |
| User-copy/focus | QEMU-VERIFIED | Invalid output rejection and six app focus transitions. |
| Framebuffer/USB/network | QEMU-VERIFIED | Window/panel, xHCI/two HID, and DHCP markers. |
| Visible FAT + GPU wiring | QEMU-VERIFIED | FAT32, display, and panel readiness in one boot. |
| Visible desktop workflow | MANUAL-VERIFIED, dated | Rocco verified create/edit/save/rename/reopen/delete on 2026-07-17. |
| Physical Raspberry Pi | UNVERIFIED | No repeatable physical boot, timer, storage, framebuffer, or input evidence. |

## Runtime execution model

EL0 processes are preemptive. EL1 helper threads are cooperative. The deferred
runtime service is a third execution mode:

```text
timer callback
  -> fixed account/rearm/publish periodic + network readiness
  -> board_irq_end()
  -> measured runtime pass
       -> periodic input/USB/GUI phase
       -> independently pending network phase
            -> consume at most 16 valid RX frames
            -> republish network readiness when the cap is reached
  -> process dispatch
  -> eret
```

EOI does not leave the exception. During the pass execution remains in EL1, the
288-byte exception frame remains on the EL1 stack, nested IRQ helpers restore the
vector's prior masked state, and EL0 remains paused.

The timer callback is bounded. Network RX has a count bound. The complete pass
still has no global duration bound.

## Runtime telemetry and budget state

The kernel-internal snapshot records:

- requests, coalescing, non-empty and empty passes, and requeues;
- last, maximum, and cumulative `CNTPCT_EL0` duration;
- passes exceeding the one-timer-interval observation threshold;
- input events produced and consumed;
- input queue depth, lifetime high-water, and overflow;
- USB HID polls reaching xHCI;
- valid virtio-net RX frames consumed;
- redraw submissions, partial-damage rectangles, and full redraws;
- network-budget exhaustion;
- pending and last-consumed work bits.

Pending readiness currently includes:

```text
RUNTIME_WORK_PERIODIC
RUNTIME_WORK_NETWORK
```

The network phase accepts at most `RUNTIME_NETWORK_FRAME_BUDGET == 16` valid
frames. Reaching the cap conservatively republishes `RUNTIME_WORK_NETWORK`.
Therefore:

- a seventeenth queued frame is processed on a later pass;
- exactly sixteen frames may produce one empty follow-up pass;
- exhaustion means the cap was reached, not that additional device work was
  positively observed.

Cooperative network polling outside the active runtime service remains unbudgeted.
It is outside the post-EOI class guarantee and is intentionally called out as a
remaining execution-model limitation.

The current virtio-net interface has no trustworthy device-drop or RX-ring
overflow counter. Consumed frames are not proof of loss-free delivery.

No syscall exposes the telemetry layout. Pending state and counters assume one
CPU and one consumer; they are not SMP-safe synchronization.

## Important fixed limits

| Area | Current limit |
|---|---|
| PMM | 128 MiB managed |
| Processes | 16 slots; eight user regions each |
| VFS | 24 nodes, four mounts, eight FDs/process, 64-byte paths |
| FAT32 | Root 8.3 files only |
| GUI | 16 windows; 32 queued events/window; 32 damage rectangles |
| Input | Shared 64-event queue; overflow counted but not prevented |
| Network RX | 16 virtio descriptors; post-EOI valid RX capped at 16/pass; device drops unavailable |
| Editor | 512-byte buffer; caret-line viewport |
| Files | `/fat` only; eight displayed root entries |
| Network API | No sockets, TCP, DNS API, or HTTP |
| USB | Direct keyboard/mouse HID; no hubs; at most four registered HID devices |
| User copy | Permission-aware but not fault-recoverable |
| RPi4 | Build/host scaffolding only |

## Risks by release impact

### Blocks formal v0.2

- **RISK-017:** network RX is bounded, but the complete runtime service is not.
- Input queue draining remains unlimited per pass.
- USB HID polling has no per-pass operation limit.
- Redraw/damage work has no per-pass limit.
- No global generic-counter deadline exists.
- No sustained-load QEMU heartbeat proves EL0 progress or no silent loss.
- No formal v0.2 tag/evidence record exists.

### Required before v1.0

- **RISK-013:** storage/VFS is too narrow.
- **RISK-014:** applications are incomplete daily tools.
- No ext2, combined reboot-persistence gate, or 30-minute stable visible session.

### Ongoing hardening and hardware

- **RISK-015:** copyin/copyout is not fault-contained.
- TTBR1, ASIDs, and scoped TLB invalidation are absent.
- **RISK-007:** no physical Raspberry Pi evidence.

## Application milestone

Issue #2 is **v0.6 useful desktop applications**, not v1.1. It depends on v0.3
paths/metadata, v0.4 real FAT, and v0.5 shared runtime/widgets.

## Promotion gates

```sh
bash tools/verify.sh
make qemu-fb-visible   # separate manual evidence
```

## Next sequence

1. Split and bound input queue consumption.
2. Bound USB HID/device polling.
3. Bound redraw/damage work.
4. Enforce a global generic-counter deadline and preserve every exhausted class.
5. Add sustained-load QEMU heartbeat and explicit loss accounting.
6. Close or accept RISK-017, record a visible pass, and promote/tag v0.2.
7. Begin v0.3 storage/VFS work.
