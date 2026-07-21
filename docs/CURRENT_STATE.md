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

The current runtime path measures aggregate duration plus selected work classes:
input produced, input consumed, successful redraws, queue depth/high-water, and
queue overflow. Work remains unbounded; no sustained-load QEMU test yet proves
EL0 progress under simultaneous input, network, and redraw pressure.

ArmoniOS is accurately described as:

> A compact AArch64 QEMU desktop alpha with freestanding EL0 applications, a
> kernel compositor, a narrow writable FAT32 workflow, and broad automated
> regression coverage.

It is not a production OS, general FAT implementation, POSIX system, or verified
Raspberry Pi operating system.

## Audit metadata

- **Audit date:** 2026-07-21
- **Primary verified platform:** QEMU `virt`, Cortex-A72 CPU model
- **Merged aggregate telemetry baseline:**
  `b3fd013da43fc3eacee153f3535a997e039245f3`
- **Work-class candidate PR:** #45
- **Validated final PR tree:**
  `2923fe133de368ace8ac5ff88342f4a9bd3dd3a4`
- **Tracking issue:** #43
- **Hosted validation:**
  - `Verify ArmoniOS` run `29831391737`: success
  - `CI - Tests` run `29831391738`: success
- **QEMU kernel size:** 107204 bytes; limit 108000; margin 796 bytes

Those runs validate code, tests, verification scripts, README, AGENTS, current
state, runtime documentation, and risk documentation. Later commits that only
record the successful run IDs do not change kernel behavior. A final merge
commit is not an independent execution unless a workflow runs on that exact SHA.

## Release phases

| Phase | State | Real interpretation |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, deterministic QEMU gates, CI, and dated manual evidence exist. |
| v0.2 cleanup/hardening | IN PROGRESS / CANDIDATE | Ownership cleanup, process lifecycle, bounded timer callback, aggregate timing, input/redraw metrics, and input overflow accounting exist. Budgets and stress proof remain. |
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
| QEMU build and size | BUILD-VERIFIED | `.data == 0`; kernel 107204 bytes under the 108000-byte ceiling. |
| RPi4 build/probe gates | BUILD/HOST-VERIFIED | Normal and diagnostic images build; unsupported normal capabilities remain fail closed. |
| Native host suite | HOST-VERIFIED | Kernel, memory, VFS, FAT32, GUI, parser, driver, and ABI tests pass. |
| Runtime service regression | HOST-VERIFIED | Timing, EOI order, coalescing, requeue, reset, indexed work last/max/total, inactive-report rejection, and queue-pressure accumulation pass. |
| Input queue telemetry | HOST-VERIFIED | Zero state, 64-entry high-water, full-queue overflow, draining, and reset pass. |
| Process/VFS/user-copy/KLI1 | HOST-VERIFIED | Parent/wait, local FDs, permission-aware copy, and mutable-storage contracts pass. |
| Stack check | HOST-VERIFIED | Editor maximum remains 368 bytes against 3072. |
| FAT32 smoke | QEMU-VERIFIED | Storage initialization and FAT application markers appear. |
| User-copy/focus | QEMU-VERIFIED | Invalid output is rejected without halt; six app focus transitions pass. |
| Framebuffer/USB/network | QEMU-VERIFIED | Window/panel, xHCI/two HID, and DHCP markers pass. |
| Visible FAT + GPU wiring | QEMU-VERIFIED | FAT32, display, and panel readiness appear in one boot. |
| Visible desktop workflow | MANUAL-VERIFIED, dated | Rocco verified create/edit/save/rename/reopen/delete on 2026-07-17. No newer visible pass is recorded. |
| Physical Raspberry Pi | UNVERIFIED | No repeatable physical boot, timer, storage, framebuffer, or input evidence. |

## Runtime execution model

EL0 processes are preemptive. EL1 helper threads are cooperative. The deferred
runtime service is a third execution mode:

```text
timer callback
  -> fixed account/rearm/publish work
  -> board_irq_end()
  -> measured runtime pass
       -> input producers
       -> input queue to GUI
       -> dirty redraw
       -> network poll
       -> aggregate and class metrics
  -> process dispatch
  -> eret
```

EOI does not leave the exception. During the pass execution remains in EL1, the
288-byte exception frame remains on the EL1 stack, nested IRQ helpers restore the
vector's prior masked state, and EL0 remains paused.

The timer callback is bounded. The complete pass is measurable but not bounded.

## Runtime telemetry

The kernel-internal snapshot records:

- requests, coalescing, non-empty/empty passes, and requeues;
- last/max/total `CNTPCT_EL0` duration and one-interval overruns;
- input produced by virtio-input and direct USB HID;
- input consumed from the shared queue during the active bottom half;
- successful redraw submissions;
- maximum queue depth, lifetime high-water, and full-queue overflow;
- pending and last-consumed work bits.

Work-class reports outside the active pass are ignored, so cooperative console
activity does not contaminate bottom-half measurements. Each class stores
last-pass, maximum-pass, and cumulative counts.

Network frames, device-operation counts, damage/full-redraw detail, class budget
exhaustion, and EL0 heartbeat progress remain unmeasured.

No syscall exposes the internal layout. The pending mask and telemetry assume one
CPU and one consumer; they are not SMP-safe synchronization.

## Important fixed limits

| Area | Current limit |
|---|---|
| PMM | 128 MiB managed |
| Processes | 16 slots; eight user regions each |
| VFS | 24 nodes, four mounts, eight FDs/process, 64-byte paths |
| FAT32 | Root 8.3 files only |
| GUI | 16 windows; 32 queued events/window |
| Input | Shared 64-event queue; overflow counted but not prevented |
| Editor | 512-byte buffer; caret-line viewport |
| Files | `/fat` only; eight displayed root entries |
| Network | No sockets, TCP, DNS API, or HTTP |
| USB | Direct keyboard/mouse HID; no hubs |
| User copy | Permission-aware but not fault-recoverable |
| RPi4 | Build/host scaffolding only |

## Risks by release impact

### Blocks formal v0.2

- **RISK-017:** selected work is measurable but not bounded.
- Network frames, device operations, damage batches, and exhaustion are not fully measured.
- No class budgets or global deadline exist.
- Budget-exhausted work has no pending-bit preservation rule.
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

1. Measure network frames, device operations, damage/full-redraw work, and drops.
2. Select class budgets and a global deadline from evidence.
3. Preserve or republish specific pending bits when a budget expires.
4. Add sustained-load QEMU heartbeat and explicit loss accounting.
5. Close or accept RISK-017, record a visible pass, and promote/tag v0.2.
6. Begin v0.3 storage/VFS work.
