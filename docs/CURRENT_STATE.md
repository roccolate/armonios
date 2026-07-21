# Current State

> Operational source of truth for ArmoniOS.
>
> Evidence terminology: `DOCUMENTATION_POLICY.md`  
> Active correctness and release risks: `TECHNICAL_RISKS.md`  
> Future intent and milestone ordering: `ROADMAP.md`

## Executive classification

ArmoniOS is a **v0.1 QEMU desktop baseline** and an active **v0.2
cleanup/runtime-hardening candidate**.

The QEMU `virt` kernel, graphical desktop, narrow writable FAT32 workflow,
freestanding EL0 applications, and automated verification matrix are real and
reproducible. Most original v0.2 cleanup work has landed.

The v0.2 candidate now measures deferred-runtime requests, coalescing, requeues,
non-empty and empty passes, last/maximum/cumulative duration, and passes that
exceed one timer interval. It is still not a formal v0.2 release because work per
pass remains unbounded and no sustained-load QEMU test proves EL0 progress under
input/network/redraw pressure.

ArmoniOS should be described publicly as:

> A compact AArch64 QEMU desktop alpha with freestanding EL0 applications, a
> kernel compositor, a narrow writable FAT32 workflow, and a broad automated
> verification baseline.

It should not be described as a production OS, a general FAT implementation, a
POSIX system, or a Raspberry Pi operating system.

## Audit metadata

- **Audit date:** 2026-07-21
- **Primary verified platform:** QEMU `virt`, Cortex-A72 CPU model
- **Main before the telemetry candidate:** `84d84cd0698a5a62c02ad4250c7fbec8adab88e6`
- **Candidate PR:** #44, deferred runtime-service telemetry
- **Validated candidate tree:** `95616b865ba021da6ff733bb54213a2db404ba9d`
- **Telemetry implementation code head:** `9b047f2e0b97291e184af9f528d1a4f128baf788`
- **Tracking issue:** #43, v0.2 measure and bound deferred runtime service
- **Hosted validation:**
  - `Verify ArmoniOS` run `29828181038`: success
  - `CI - Tests` run `29828181039`: success

Those runs validated the complete candidate containing code, tests, README,
AGENTS, runtime documentation, current status, and risk updates. Later
 evidence-only commits that merely record the successful run IDs do not change
the kernel or its verified behavior. A final merge commit must not be described
as independently tested unless GitHub runs a separate workflow against that
exact commit.

## Release-phase status

| Phase | State | Real interpretation |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, deterministic QEMU gates, CI, and dated manual evidence exist. |
| v0.2 cleanup/hardening | IN PROGRESS / CANDIDATE | Syscall ownership, VFS decoupling, fail-closed RPi behavior, process lifecycle, bounded timer callback, and aggregate runtime timing telemetry are implemented. Work budgets and stress proof remain. |
| v0.3 storage/VFS platform | NEXT AFTER v0.2 | Mount callbacks, MBR parsing, and block views exist, but there is no common path resolver, rich block metadata, or structured filesystem ABI. |
| v0.4 real FAT | PLANNED | FAT remains root-only 8.3 FAT32. |
| v0.5 userland runtime/widgets | PLANNED | No reusable heap, dynamic containers, or widget toolkit exists. |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Seven apps run, but issue #2's daily-use workflows are not implemented. |
| v0.7 ext2 | PLANNED | No ext2 implementation exists. |
| v0.8 polish | EARLY PARTIAL | Basic window/panel behavior exists; sustained visible-session evidence does not. |
| v0.9 beta | NOT STARTED | No ABI freeze, fuzz campaign, reboot-persistence gate, or beta record exists. |
| v1.0 | NOT READY | Storage, apps, ext2, persistence, runtime bounds, and final evidence are incomplete. |

## Verification record

| Check | Evidence class | Result and scope |
|---|---|---|
| `make BOARD=qemu_virt` | BUILD-VERIFIED | Kernel and seven KLI1 applications build for QEMU. |
| `make BOARD=qemu_virt size` | BUILD-VERIFIED | Kernel preserves `.data == 0` and the 108000-byte binary limit with telemetry present. |
| `make -C tests test` | HOST-VERIFIED | Native kernel, memory, VFS, FAT32, GUI, parser, driver, and ABI tests pass. |
| `bash tests/run_runtime_service_test.sh` | HOST-VERIFIED | Coalescing, EOI order, requeue preservation/counting, deterministic timing, last/max/total duration, interval overrun counting, snapshot state, null snapshot handling, and reset behavior pass. |
| Process/VFS/user-copy/KLI1 gates | HOST-VERIFIED | Parent/wait, process-local descriptors, permission-aware copy, and mutable-storage contracts pass. |
| RPi4 build/probe gates | BUILD/HOST-VERIFIED | Normal and diagnostic images build; unsupported normal capabilities remain fail closed. |
| `make stack-check` | HOST-VERIFIED | Recorded maximum remains 368 bytes in Editor against a 3072-byte limit. |
| `make qemu-fs-test` | QEMU-VERIFIED | Storage initialization and FAT application markers appear. |
| User-copy/focus QEMU gates | QEMU-VERIFIED | Invalid output is rejected without halting; six app focus transitions pass. |
| Framebuffer/USB/network gates | QEMU-VERIFIED | Window/panel, xHCI/two HID, and DHCP markers pass. |
| Visible FAT + GPU wiring | QEMU-VERIFIED | FAT32, display, and panel readiness appear in one boot. |
| `make qemu-fb-visible` | MANUAL-VERIFIED, dated | Rocco verified create/edit/save/rename/reopen/delete on 2026-07-17. No newer visible pass is recorded. |
| Physical Raspberry Pi boot | UNVERIFIED | No repeatable physical boot, timer, storage, framebuffer, or input evidence exists. |

## Runtime architecture

### EL0 and EL1 execution

EL0 processes are preemptive. EL1 helper threads are cooperative. The deferred
runtime service is a third execution mode:

```text
timer IRQ callback
  -> tick accounting and CNTP_CVAL rearm
  -> publish RUNTIME_WORK_PERIODIC
  -> scheduler accounting
  -> board_irq_end()
  -> runtime_service_run_pending()
       -> read CNTPCT_EL0
       -> poll input/devices, route GUI, redraw, poll network
       -> read CNTPCT_EL0 and update telemetry
  -> process dispatch
  -> eret
```

EOI releases the interrupt controller but does not leave the exception. During
the service pass, execution remains in EL1, normal IRQs remain masked, the
288-byte exception frame stays on the EL1 stack, and EL0 remains paused.

The physical timer callback is bounded. The complete exception path is measured
but not yet bounded.

### Runtime telemetry

The kernel-internal snapshot records:

- accepted and coalesced requests;
- non-empty and empty consumer invocations;
- passes that republish work;
- last, maximum, and cumulative generic-counter duration;
- passes exceeding the configured timer interval;
- counter frequency, threshold, pending bits, and last consumed bits.

Production timing uses `CNTPCT_EL0`; `CNTFRQ_EL0` provides the conversion
frequency. The initial observation threshold is one timer interval—about 10 ms
at 100 Hz. It detects a serious overrun but is not the final accepted budget.

No syscall exposes this internal structure. Pending publication and telemetry
remain valid only under the current single-core, IRQ-masked, one-consumer model.

## Important fixed limits

| Area | Current limit |
|---|---|
| PMM | At most 128 MiB managed |
| Processes | 16 slots; eight user regions each |
| VFS | 24 nodes, four mounts, eight FDs/process, 64-byte paths |
| FAT32 | Root 8.3 files only; no directories/LFN/general FAT claim |
| GUI | 16 windows; 32 queued events/window |
| Input | Shared 64-event producer queue |
| Editor | 512-byte buffer; renders the caret line only |
| Files | `/fat` only; eight displayed root entries |
| Network | No sockets, TCP, DNS API, or HTTP |
| USB | Direct keyboard/mouse HID; no hubs |
| User copy | Permission-aware but not fault-recoverable |
| RPi4 | Build/host scaffolding only; no physical claim |

## Open risks by release impact

### Blocks formal v0.2 promotion

- **RISK-017:** aggregate duration is measured, but work remains unbounded.
- Per-class queue, input, device, packet, redraw, overflow, and budget metrics are absent.
- No sustained-load QEMU heartbeat proves EL0 progress or explicit loss accounting.
- No formal v0.2 tag/evidence record exists.

### Required before v1.0

- **RISK-013:** storage/VFS platform is too narrow.
- **RISK-014:** applications are not complete daily tools.
- No ext2 implementation, combined reboot-persistence gate, or 30-minute stable visible session exists.

### Ongoing hardening and hardware

- **RISK-015:** copyin/copyout is not fault-contained.
- TTBR1, ASIDs, and scoped TLB invalidation are absent.
- **RISK-007:** no physical Raspberry Pi evidence exists.

## Application milestone correction

Issue #2 is **v0.6 useful desktop applications**, not v1.1. It depends on v0.3
common paths/metadata, v0.4 real FAT, and v0.5 shared runtime/widgets. Small
isolated usability fixes may land earlier but must not replace those foundations.

## Promotion gate

```sh
bash tools/verify.sh
```

Manual visible claims additionally require:

```sh
make qemu-fb-visible
```

Record tester, date, exact commit, workflow, result, and limitations.

## Next technically correct sequence

1. Add per-class input, device, packet, redraw, queue, and overflow metrics.
2. Use measurements to define independent work budgets and a global deadline.
3. Preserve or republish specific pending bits when a budget expires.
4. Add sustained-load QEMU tests proving EL0 heartbeat and explicit loss accounting.
5. Close or accept RISK-017, record a visible pass, and promote/tag v0.2.
6. Begin v0.3 storage/VFS platform work.
