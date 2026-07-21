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

The merged v0.2 runtime foundation measures aggregate service duration,
requests, coalescing, requeue, and timer-interval overruns. PR #45 adds a
candidate second measurement layer for input work, redraws, and shared queue
pressure. Work remains unbounded; no sustained-load QEMU test yet proves EL0
progress under simultaneous input, network, and redraw pressure.

ArmoniOS should be described publicly as:

> A compact AArch64 QEMU desktop alpha with freestanding EL0 applications, a
> kernel compositor, a narrow writable FAT32 workflow, and a broad automated
> verification baseline.

It is not a production OS, general FAT implementation, POSIX system, or verified
Raspberry Pi operating system.

## Audit metadata

- **Audit date:** 2026-07-21
- **Primary verified platform:** QEMU `virt`, Cortex-A72 CPU model
- **Current merged runtime telemetry baseline:**
  `b3fd013da43fc3eacee153f3535a997e039245f3`
- **Active measurement PR:** #45, runtime input/redraw work metrics
- **Candidate implementation tree before documentation:**
  `0febb1bf2303caa079a54ad4344fb04acb275507`
- **Tracking issue:** #43, v0.2 measure and bound deferred runtime service
- **Candidate evidence currently recorded:**
  - `Verify ArmoniOS` run `29830679366`: success
  - `CI - Tests` run `29830679347`: final result must be recorded before merge
- **Candidate QEMU kernel size:** 107204 bytes; limit 108000

The successful short workflow covers the implementation and tests before the
current documentation commits. The complete final PR tree must receive hosted
validation before merge. A merge commit is not an independent test execution
unless GitHub runs a workflow against that exact commit.

## Release phases

| Phase | State | Real interpretation |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, deterministic QEMU gates, CI, and dated manual evidence exist. |
| v0.2 cleanup/hardening | IN PROGRESS / CANDIDATE | Syscall ownership, VFS decoupling, fail-closed RPi behavior, process lifecycle, bounded timer callback, aggregate timing, and candidate input/redraw metrics exist. Budgets and stress proof remain. |
| v0.3 storage/VFS platform | NEXT AFTER v0.2 | Mount callbacks, MBR parsing, and block views exist, but there is no common path resolver, rich block metadata, or structured filesystem ABI. |
| v0.4 real FAT | PLANNED | Current FAT remains root-only 8.3 FAT32. |
| v0.5 userland runtime/widgets | PLANNED | No reusable heap, dynamic containers, or shared widget toolkit exists. |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Seven apps run, but issue #2's daily-use workflows are incomplete. |
| v0.7 ext2 | PLANNED | No ext2 implementation exists. |
| v0.8 polish | EARLY PARTIAL | Basic windows/panel behavior exists; sustained visible-session evidence does not. |
| v0.9 beta | NOT STARTED | No ABI freeze, fuzz campaign, reboot-persistence gate, or beta record exists. |
| v1.0 | NOT READY | Storage, apps, ext2, persistence, runtime bounds, and final evidence are incomplete. |

## Verification record

| Check | Evidence class | Result and scope |
|---|---|---|
| `make BOARD=qemu_virt` | BUILD-VERIFIED | Kernel and seven KLI1 applications build. |
| `make BOARD=qemu_virt size` | BUILD-VERIFIED | `.data == 0`; candidate binary is 107204 bytes under the 108000-byte limit. |
| `make -C tests test` | HOST-VERIFIED | Native kernel, memory, VFS, FAT32, GUI, parser, driver, and ABI tests pass on the candidate short workflow. |
| `bash tests/run_runtime_service_test.sh` | HOST-VERIFIED | Timing, coalescing, EOI order, requeue, reset, indexed work last/max/total, inactive-report rejection, and queue-pressure accumulation pass. |
| `bash tests/run_input_queue_stats_test.sh` | HOST-VERIFIED | Depth, 64-event high-water, overflow rejection/counting, draining, and reset pass. |
| Process/VFS/user-copy/KLI1 gates | HOST-VERIFIED | Parent/wait, process-local descriptors, permission-aware copy, and mutable-storage contracts pass. |
| RPi4 build/probe gates | BUILD/HOST-VERIFIED | Normal and diagnostic images build; unsupported normal capabilities remain fail closed. |
| `make stack-check` | HOST-VERIFIED | Recorded maximum remains 368 bytes in Editor against 3072 bytes. |
| `make qemu-fs-test` | QEMU-VERIFIED | Storage initialization and FAT application markers appear. |
| User-copy/focus QEMU gates | QEMU-VERIFIED | Invalid output is rejected without halting; six app focus transitions pass. |
| Framebuffer/USB/network gates | QEMU-VERIFIED | Window/panel, xHCI/two HID, and DHCP markers pass on the merged baseline. |
| Visible FAT + GPU wiring | QEMU-VERIFIED | FAT32, display, and panel readiness appear in one boot. |
| `make qemu-fb-visible` | MANUAL-VERIFIED, dated | Rocco verified create/edit/save/rename/reopen/delete on 2026-07-17. No newer visible pass is recorded. |
| Physical Raspberry Pi boot | UNVERIFIED | No repeatable physical boot, timer, storage, framebuffer, or input evidence exists. |

## Runtime execution model

EL0 processes are preemptive. EL1 helper threads are cooperative. The deferred
runtime service is a third execution mode:

```text
timer callback
  -> account/rearm/publish
  -> board_irq_end()
  -> runtime_service_run_pending()
       -> measure CNTPCT_EL0 duration
       -> poll input producers
       -> consume input into GUI
       -> redraw when dirty
       -> poll network
       -> record aggregate and work-class metrics
  -> process dispatch
  -> eret
```

EOI does not leave the exception. During the pass execution remains in EL1, the
288-byte exception frame remains on the EL1 stack, and nested IRQ helpers restore
the vector's prior masked state. EL0 remains paused.

The timer callback is bounded. The complete pass is measurable but not bounded.

## Runtime telemetry

### Merged aggregate layer

The kernel-internal snapshot records requests, coalescing, non-empty/empty
passes, requeues, last/max/total generic-counter duration, timer-interval
overruns, frequency, threshold, pending bits, and last-consumed bits.

At 100 Hz, one timer interval is approximately 10 ms. That is an observation
threshold, not the final accepted budget.

### Candidate work-class layer

PR #45 adds compact indexed measurements for:

- input events produced by virtio-input and direct USB HID;
- input events consumed from the shared queue during the active bottom-half pass;
- successful display redraw submissions;
- maximum queue depth observed during a pass;
- lifetime queue high-water;
- rejected pushes caused by the full 64-event queue.

Each work class stores last-pass, maximum-pass, and cumulative counts. Reports
outside the active runtime-service pass are ignored so the cooperative console
thread does not contaminate the measurements.

Network frame counts, device-operation counts, damage/full-redraw detail, budget
exhaustion, and EL0 heartbeat progress remain unmeasured.

No syscall exposes the internal telemetry layout. It remains valid only under
the current single-core, one-consumer assumptions.

## Important fixed limits

| Area | Current limit |
|---|---|
| PMM | At most 128 MiB managed |
| Processes | 16 slots; eight user regions each |
| VFS | 24 nodes, four mounts, eight FDs/process, 64-byte paths |
| FAT32 | Root 8.3 files only; no directories/LFN/general FAT claim |
| GUI | 16 windows; 32 queued events/window |
| Input | Shared 64-event queue; overflow now counted in the candidate |
| Editor | 512-byte buffer; renders only the caret line |
| Files | `/fat` only; eight displayed root entries |
| Network | No sockets, TCP, DNS API, or HTTP |
| USB | Direct keyboard/mouse HID; no hubs |
| User copy | Permission-aware but not fault-recoverable |
| RPi4 | Build/host scaffolding only; no physical claim |

## Risks by release impact

### Blocks formal v0.2

- **RISK-017:** duration and selected work classes are measurable, but work is
  not bounded.
- Network frames, device operations, damage batches, and budget exhaustion remain
  unmeasured.
- No per-class or global deadline is enforced.
- No sustained-load QEMU heartbeat proves EL0 progress or no silent loss.
- No formal v0.2 tag/evidence record exists.

### Required before v1.0

- **RISK-013:** storage/VFS platform is too narrow.
- **RISK-014:** desktop applications are incomplete daily tools.
- No ext2 implementation, combined reboot-persistence gate, or 30-minute stable
  visible session exists.

### Ongoing hardening and hardware

- **RISK-015:** copyin/copyout is not fault-contained.
- TTBR1, ASIDs, and scoped TLB invalidation are absent.
- **RISK-007:** no physical Raspberry Pi evidence exists.

## Application milestone

Issue #2 is **v0.6 useful desktop applications**, not v1.1. It depends on v0.3
paths/metadata, v0.4 real FAT, and v0.5 shared runtime/widgets.

## Promotion gates

```sh
bash tools/verify.sh
make qemu-fb-visible   # separate manual evidence
```

Record tester, date, exact commit, workflow, result, and limitations.

## Next sequence

1. Measure network frames, device operations, damage/full-redraw work, and drops.
2. Use measurements to select independent class budgets and a global deadline.
3. Preserve or republish specific pending bits when a budget expires.
4. Add sustained-load QEMU tests proving EL0 heartbeat and explicit loss accounting.
5. Close or accept RISK-017, record a visible pass, and promote/tag v0.2.
6. Begin v0.3 storage/VFS work.
