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

Runtime measurement Phase 1B is complete. Phase 2 now has two enforced count
budgets:

- shared input-queue consumption: at most 16 events per active post-EOI pass;
- virtio-net RX consumption: at most 16 valid frames per active network pass.

Input and network readiness are independently pending. Input requeues only when
the queue still contains events. Network requeues conservatively whenever its cap
is reached.

The service is not fully bounded. Input producer/USB polling, redraw/damage work,
and total generic-counter time remain unlimited per pass. No sustained-load QEMU
test proves EL0 progress under combined pressure.

ArmoniOS is accurately described as:

> A compact AArch64 QEMU desktop alpha with freestanding EL0 applications, a
> kernel compositor, a narrow writable FAT32 workflow, broad automated regression
> coverage, and two bounded runtime work classes.

It is not a production OS, general FAT implementation, POSIX system, or verified
Raspberry Pi operating system.

## Audit metadata

- **Audit date:** 2026-07-21
- **Primary verified platform:** QEMU `virt`, Cortex-A72 CPU model
- **Tracking issue:** #43, v0.2 measure and bound deferred runtime service
- **Network budget merge:**
  `3797f7e7cf3dfb825d927e399aa4769b27020e29`
- **Input budget merge:**
  `41f3e185ca1f75ed09416313d34279384f3d78a9`
- **Latest validated implementation head:**
  `ba8051cd8edbe6a66a843f80c54c96668d064a91`
- **Hosted validation:**
  - `Verify ArmoniOS` run `29853659559`: success
  - `CI - Tests` run `29853659491`: success
- **Loadable QEMU kernel size:** 107802 bytes
- **Kernel size limit:** 108000 bytes
- **Remaining margin:** 198 bytes

Those runs validate the input and network budgets, deterministic continuation
contracts, strict runtime-service short-workflow gate, QEMU behavior, and all
pre-existing subsystem gates. The merge commit is not a separately executed
workflow unless a run targets that exact SHA.

## Release phases

| Phase | State | Real interpretation |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, deterministic QEMU gates, CI, and dated manual evidence exist. |
| v0.2 cleanup/hardening | IN PROGRESS / CANDIDATE | Observable work is measured; input consumption and network RX are bounded. USB producers, redraw, global time, and stress proof remain. |
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
| QEMU build and size | BUILD-VERIFIED | `.data == 0`; loadable kernel 107802 bytes under the 108000-byte ceiling. |
| RPi4 build/probe gates | BUILD/HOST-VERIFIED | Normal and diagnostic images build; unsupported normal capabilities fail closed. |
| Native host suite | HOST-VERIFIED | Kernel, memory, VFS, FAT32, GUI, parser, driver, and ABI tests pass. |
| Runtime service regression | HOST-VERIFIED | Timing, EOI order, coalescing, reset, all class metrics, 16-event input cap, 17-event continuation, 16-frame network cap, conservative network follow-up, and outside-service behavior pass. |
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
  -> fixed account/rearm/publish PERIODIC | INPUT | NETWORK
  -> board_irq_end()
  -> measured runtime pass
       -> periodic producer work with INPUT phase active
            -> consume at most 16 queue events
            -> requeue only if queue work remains
       -> independently pending NETWORK phase
            -> consume at most 16 valid RX frames
            -> conservatively requeue at the cap
  -> process dispatch
  -> eret
```

EOI does not leave the exception. During the pass execution remains in EL1, the
288-byte exception frame remains on the EL1 stack, nested IRQ helpers restore the
vector's prior masked state, and EL0 remains paused.

The timer callback is bounded. Input consumption and network RX have count bounds.
The complete pass still has no global duration bound.

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
- input- and network-budget exhaustion;
- pending and last-consumed work bits.

Pending readiness currently includes:

```text
RUNTIME_WORK_PERIODIC
RUNTIME_WORK_INPUT
RUNTIME_WORK_NETWORK
```

### Input budget

`RUNTIME_INPUT_EVENT_BUDGET == 16` is one quarter of the fixed 64-event queue.
At the cap, the wrapper queries queue depth without consuming:

- exactly 16 events and an empty queue finish without requeue;
- a seventeenth event keeps `RUNTIME_WORK_INPUT` pending;
- exhaustion means the cap was reached while queue work remained.

Console-thread and other queue consumers outside the active runtime service pass
through without this budget. Queue overflow is counted but not prevented.

### Network budget

`RUNTIME_NETWORK_FRAME_BUDGET == 16` matches the current RX descriptor count.
Reaching the cap conservatively republishes `RUNTIME_WORK_NETWORK`:

- a seventeenth queued frame is processed later;
- exactly sixteen frames may produce one empty follow-up pass;
- exhaustion means the cap was reached, not that more device work was observed.

Cooperative network polling outside the active runtime service remains unbudgeted.
The current interface has no trustworthy device-drop or RX-ring overflow counter.

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
| Input | Shared 64-event queue; post-EOI consumption capped at 16/pass; overflow counted |
| Network RX | 16 descriptors; post-EOI valid RX capped at 16/pass; device drops unavailable |
| Kernel size | 107802 / 108000 bytes; 198 bytes remain |
| Editor | 512-byte buffer; caret-line viewport |
| Files | `/fat` only; eight displayed root entries |
| Network API | No sockets, TCP, DNS API, or HTTP |
| USB | Direct keyboard/mouse HID; no hubs; at most four registered HID devices |
| User copy | Permission-aware but not fault-recoverable |
| RPi4 | Build/host scaffolding only |

## Risks by release impact

### Blocks formal v0.2

- **RISK-017:** input consumption and network RX are bounded, but the complete
  runtime service is not.
- Input producer and USB HID polling have no per-pass operation limit.
- Redraw/damage work has no per-pass limit.
- No global generic-counter deadline exists.
- Only 198 bytes remain under the kernel ceiling; the next cut needs compaction.
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

1. Compact runtime state while preserving the current contracts and size ceiling.
2. Split and bound USB HID/device polling.
3. Bound redraw/damage work.
4. Enforce a global generic-counter deadline and preserve every exhausted class.
5. Add sustained-load QEMU heartbeat and explicit loss accounting.
6. Close or accept RISK-017, record a visible pass, and promote/tag v0.2.
7. Begin v0.3 storage/VFS work.
