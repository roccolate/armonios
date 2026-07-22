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

Runtime measurement Phase 1B is complete for the work classes currently
observable on QEMU. The service measures duration, input production and
consumption, input queue pressure, USB HID polling operations, consumed network
frames, redraw submissions, partial-damage items, and full redraws.

The work is still unbounded. No per-class budgets or global deadline exist, and
no sustained-load QEMU test proves EL0 progress under combined input, network,
and redraw pressure.

ArmoniOS is accurately described as:

> A compact AArch64 QEMU desktop alpha with freestanding EL0 applications, a
> kernel compositor, a narrow writable FAT32 workflow, and broad automated
> regression coverage.

It is not a production OS, general FAT implementation, POSIX system, or verified
Raspberry Pi operating system.

## Audit metadata

- **Audit date:** 2026-07-21
- **Primary verified platform:** QEMU `virt`, Cortex-A72 CPU model
- **Tracking issue:** #43, v0.2 measure and bound deferred runtime service
- **Aggregate telemetry merge:**
  `b3fd013da43fc3eacee153f3535a997e039245f3`
- **Input/redraw/queue telemetry merge:**
  `a3b9b447839b310c26ba99b8777d2b26a40eba09`
- **Network frame telemetry merge:**
  `f60ab2884b91eb5f57a2c1a8c10f7cbb18c769bb`
- **USB HID poll telemetry merge:**
  `7a6780d3091c82998f8da08d8fa4c85640f365b5`
- **Damage/full-redraw telemetry merge:**
  `f327868fe2439772b79cd004432387dc80bc21a5`
- **Latest fully validated code head:**
  `6634c3a6f527433643a56f2c90cc6af8bad62c1d`
- **Hosted validation:**
  - `Verify ArmoniOS` run `29840410727`: success
  - `CI - Tests` run `29840411044`: success
- **Loadable QEMU kernel size:** 107370 bytes
- **Kernel size limit:** 108000 bytes
- **Remaining margin:** 630 bytes

Those runs validate the Phase 1B code and tests before this documentation-only
synchronization. The squash merge commit is not a separately executed workflow
unless GitHub runs against that exact SHA.

## Release phases

| Phase | State | Real interpretation |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, deterministic QEMU gates, CI, and dated manual evidence exist. |
| v0.2 cleanup/hardening | IN PROGRESS / CANDIDATE | Ownership cleanup, process lifecycle, bounded timer callback, and complete observable-work measurement exist. Budgets and stress proof remain. |
| v0.3 storage/VFS platform | NEXT AFTER v0.2 | No common path resolver, rich block metadata, or structured filesystem ABI. |
| v0.4 real FAT | PLANNED | Current FAT remains root-only 8.3 FAT32. |
| v0.5 userland runtime/widgets | PLANNED | No reusable heap, dynamic containers, or shared widget toolkit. |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Seven apps run, but issue #2's daily-use workflows are incomplete. |
| v0.7 ext2 | PLANNED | No ext2 implementation. |
| v0.8 polish | EARLY PARTIAL | Basic window/panel behavior exists; sustained visible-session evidence does not. |
| v0.9 beta | NOT STARTED | No ABI freeze, fuzz campaign, reboot-persistence gate, or beta record. |
| v1.0 | NOT READY | Storage, apps, ext2, persistence, runtime bounds, and final evidence are incomplete. |

## Verification record

| Check | Evidence class | Result and scope |
|---|---|---|
| QEMU build and size | BUILD-VERIFIED | `.data == 0`; loadable kernel 107370 bytes under the 108000-byte ceiling. |
| RPi4 build/probe gates | BUILD/HOST-VERIFIED | Normal and diagnostic images build; unsupported normal capabilities fail closed. |
| Native host suite | HOST-VERIFIED | Kernel, memory, VFS, FAT32, GUI, parser, driver, and ABI tests pass. |
| Runtime service regression | HOST-VERIFIED | Timing, EOI order, coalescing, requeue, reset, all indexed class metrics, direct partial/full helper behavior, inactive-report rejection, queue pressure, and static wiring pass. |
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
  -> fixed account/rearm/publish work
  -> board_irq_end()
  -> measured runtime pass
       -> virtio-input and USB HID producers
       -> input queue to GUI
       -> dirty redraw and compositor batch report
       -> virtio-net RX drain
       -> aggregate and per-class metrics
  -> process dispatch
  -> eret
```

EOI does not leave the exception. During the pass execution remains in EL1, the
288-byte exception frame remains on the EL1 stack, nested IRQ helpers restore the
vector's prior masked state, and EL0 remains paused.

The timer callback is bounded. The complete pass is measurable but not bounded.

## Runtime telemetry

The kernel-internal snapshot records:

- requests, coalescing, non-empty and empty passes, and requeues;
- last, maximum, and cumulative `CNTPCT_EL0` duration;
- passes exceeding the one-timer-interval observation threshold;
- input events produced by virtio-input and directly attached USB HID;
- input events consumed from the shared queue;
- maximum queue depth, lifetime high-water, and full-queue overflow;
- USB HID polls that reach the active xHCI controller;
- valid virtio-net RX frames consumed;
- successful QEMU redraw submissions;
- merged partial-damage rectangles submitted in successful redraws;
- full-redraw fallbacks submitted successfully;
- pending and last-consumed work bits.

Each indexed class stores last-pass, maximum-pass, and cumulative values. Reports
outside the active pass are ignored, so cooperative console work does not
contaminate bottom-half measurements.

The network metric counts frames software actually consumed. It does not prove
that the 16-descriptor RX ring never lost work; the current device interface has
no trustworthy device-drop or ring-overflow signal.

Pixels, damaged area, GPU completion latency, and failed-draw CPU work are not
measured. These omissions do not prevent choosing initial count-based budgets,
but must remain explicit.

No syscall exposes the internal telemetry layout. Pending state and telemetry
assume one CPU and one consumer; they are not SMP-safe synchronization.

## Important fixed limits

| Area | Current limit |
|---|---|
| PMM | 128 MiB managed |
| Processes | 16 slots; eight user regions each |
| VFS | 24 nodes, four mounts, eight FDs/process, 64-byte paths |
| FAT32 | Root 8.3 files only |
| GUI | 16 windows; 32 queued events/window; 32 damage rectangles |
| Input | Shared 64-event queue; overflow counted but not prevented |
| Network RX | 16 virtio descriptors; consumed frames measured, device drops unavailable |
| Editor | 512-byte buffer; caret-line viewport |
| Files | `/fat` only; eight displayed root entries |
| Network API | No sockets, TCP, DNS API, or HTTP |
| USB | Direct keyboard/mouse HID; no hubs; at most four registered HID devices |
| User copy | Permission-aware but not fault-recoverable |
| RPi4 | Build/host scaffolding only |

## Risks by release impact

### Blocks formal v0.2

- **RISK-017:** work is measurable but not bounded.
- Only one periodic pending bit exists; work classes cannot yet be resumed
  independently.
- No input, network, USB, redraw, or global time budget exists.
- Budget-exhausted work has no class-specific pending-bit preservation rule.
- No budget-exhaustion counters exist.
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

1. Split periodic work into independently pending input, GUI, network, and device
   classes.
2. Add a bounded virtio-net receive pass first, preserving network pending work
   when the frame limit is reached.
3. Add input, USB, and redraw budgets plus a global generic-counter deadline.
4. Count every budget exhaustion and preserve the relevant pending bit.
5. Add sustained-load QEMU heartbeat and explicit loss accounting.
6. Close or accept RISK-017, record a visible pass, and promote/tag v0.2.
7. Begin v0.3 storage/VFS work.
