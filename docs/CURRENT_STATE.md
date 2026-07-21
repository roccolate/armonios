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

Runtime measurement Phase 1B is complete. Phase 2 now enforces count bounds for
post-EOI network RX, shared input consumption, virtio-input descriptor draining,
USB HID device visits, and partial compositor damage.

The service is not fully bounded. A full redraw remains one potentially
expensive operation, total generic-counter time has no enforced deadline,
cooperative network polling outside the bottom half is outside the network
guarantee, and no sustained-load QEMU test proves EL0 progress under combined
pressure.

ArmoniOS is accurately described as:

> A compact AArch64 QEMU desktop alpha with freestanding EL0 applications, a
> kernel compositor, a narrow writable FAT32 workflow, broad automated
> regression coverage, and count-bounded input, partial redraw, and post-EOI
> network work.

It is not a production OS, general FAT implementation, POSIX system, or verified
Raspberry Pi operating system.

## Audit metadata

- **Audit date:** 2026-07-21
- **Primary verified platform:** QEMU `virt`, Cortex-A72 CPU model
- **Tracking issue:** #43, v0.2 measure and bound deferred runtime service
- **Partial-redraw merge:** `fe4f2a622f5633e55b0eddb2f8f6767453a9ddca`
- **Validated implementation head:**
  `8b86a8c24f25af0937f1df2e983c1c7c4f489b7d`
- **Hosted validation:**
  - `Verify ArmoniOS` run `29863653280`: success
  - `CI - Tests` run `29863653209`: success
- **Loadable QEMU kernel size:** 107982 bytes
- **Kernel size limit:** 108000 bytes
- **Remaining margin:** 18 bytes

Those runs validate the implementation, deterministic producer/consumer/redraw
budget regressions, runtime-service gate, QEMU behavior, and all pre-existing
subsystem gates. A merge commit is not a separately executed workflow unless a
run targets that exact SHA.

## Release phases

| Phase | State | Real interpretation |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, deterministic QEMU gates, CI, and dated manual evidence exist. |
| v0.2 cleanup/hardening | IN PROGRESS / CANDIDATE | Input, network, and partial-damage counts are bounded. Global time and stress proof remain. |
| v0.3 storage/VFS platform | NEXT AFTER v0.2 | No common path resolver, rich block metadata, or structured filesystem ABI. |
| v0.4 real FAT | PLANNED | Current FAT remains root-only 8.3 FAT32. |
| v0.5 userland runtime/widgets | PLANNED | No reusable heap, dynamic containers, or shared widget toolkit. |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Seven apps run, but issue #2's daily-use workflows are incomplete. |
| v0.7 ext2 | PLANNED | No ext2 implementation. |
| v0.8 polish | EARLY PARTIAL | Basic window/panel behavior exists; sustained visible-session evidence does not. |
| v0.9 beta | NOT STARTED | No ABI freeze, fuzz campaign, reboot-persistence gate, or beta record. |
| v1.0 | NOT READY | Storage, apps, ext2, persistence, remaining runtime proof, and final evidence are incomplete. |

## Verification record

| Check | Evidence class | Result and scope |
|---|---|---|
| QEMU build and size | BUILD-VERIFIED | `.data == 0`; loadable kernel 107982 bytes under the 108000-byte ceiling. |
| RPi4 build/probe gates | BUILD/HOST-VERIFIED | Normal and diagnostic images build; unsupported normal capabilities fail closed. |
| Native host suite | HOST-VERIFIED | Kernel, memory, VFS, FAT32, GUI, parser, driver, and ABI tests pass. |
| Runtime service regression | HOST-VERIFIED | Timing, EOI order, coalescing, reset, class metrics, input/network caps, and redraw batches pass. |
| Virtio-input producer | HOST-VERIFIED | Ten used descriptors on an eight-entry ring complete as 8 + 2 with ten queued events. |
| USB HID producer | HOST-VERIFIED | A malformed count of 255 still visits exactly the fixed four HID slots. |
| Partial redraw | HOST-VERIFIED | Twenty rectangles complete as 8 + 8 + 4; failed GPU submission preserves all damage; full redraw clears once. |
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
       -> periodic producer/GUI phase
            -> virtio-input <= min(negotiated ring, 16) descriptors
            -> USB HID <= 4 registered device visits
            -> input queue <= 16 events when INPUT is pending
            -> partial damage <= 8 rectangles/successful redraw
            -> full redraw = one operation
       -> independent network phase
            -> <= 16 valid RX frames
            -> conservative requeue at cap
  -> process dispatch
  -> eret
```

EOI does not leave the exception. During the pass execution remains in EL1, the
288-byte exception frame remains on the EL1 stack, nested IRQ helpers restore the
vector's prior masked state, and EL0 remains paused.

## Runtime telemetry and budget state

The kernel-internal snapshot records requests, coalescing, non-empty and empty
passes, requeues, last/max/total `CNTPCT_EL0` duration, interval overruns, input
production/consumption, queue pressure/overflow, USB polls, network frames,
redraw/damage shape, redraw exhaustion, budget exhaustion, pending work, and
last-consumed work.

Current enforced bounds:

```text
virtio-input producer <= min(queue_size, 16) used descriptors/call
USB HID producer      <= 4 registered device visits/call
input consumer        <= 16 queued events/post-EOI pass
partial redraw        <= 8 damage rectangles/successful redraw
network RX            <= 16 valid frames/post-EOI pass
```

Virtio-input continuation remains in its used ring. USB scans every supported
fixed slot. Input requeues only when queue events remain. Partial damage remains
ordered in the compositor list and dirty until later successful batches; a
failed redraw consumes nothing. Network conservatively requeues at 16, which can
cause one empty follow-up pass.

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
| GUI | 16 windows; 32 queued events/window; 32 damage rectangles; eight partial rectangles/redraw |
| Input queue | 64 events; overflow counted but not prevented |
| Virtio input | Ring up to 16; one negotiated ring-length drained/call |
| USB | Four direct HID devices; no hubs; four visits/call |
| Network RX | 16 descriptors; post-EOI valid RX capped at 16/pass; device drops unavailable |
| Editor | 512-byte buffer; caret-line viewport |
| Files | `/fat` only; eight displayed root entries |
| Network API | No sockets, TCP, DNS API, or HTTP |
| User copy | Permission-aware but not fault-recoverable |
| RPi4 | Build/host scaffolding only |

## Risks by release impact

### Blocks formal v0.2

- **RISK-017:** input, network, and partial-damage counts are bounded, but the
  complete runtime service is not.
- A full redraw remains one potentially expensive operation.
- No global generic-counter deadline exists.
- Cooperative network polling outside the service is unbudgeted.
- No sustained-load QEMU heartbeat proves EL0 progress or no silent loss.
- Only 18 bytes remain under the kernel ceiling; another compaction is required.
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

1. Compact runtime/kernel code again without changing behavior.
2. Enforce a service-wide generic-counter deadline and preserve unfinished work.
3. Add sustained-load QEMU heartbeat and explicit loss accounting.
4. Close or accept RISK-017, record a visible pass, and promote/tag v0.2.
5. Begin v0.3 storage/VFS work.
