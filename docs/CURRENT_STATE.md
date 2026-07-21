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
reproducible. Most of the original v0.2 cleanup work has landed.

The current v0.2 candidate now measures deferred-runtime requests, coalescing,
requeues, pass duration, maximum duration, cumulative duration, and passes that
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
- **Current main before this candidate:** `84d84cd0698a5a62c02ad4250c7fbec8adab88e6`
- **Candidate PR:** #44, deferred runtime-service telemetry
- **Telemetry implementation code head:** `9b047f2e0b97291e184af9f528d1a4f128baf788`
- **Tracking issue:** #43, v0.2 measure and bound deferred runtime service
- **Hosted evidence for the candidate:**
  - `Verify ArmoniOS` run `29827738752`: success
  - `CI - Tests` run `29827738742`: final result must be recorded before merge

The successful `Verify ArmoniOS` run covers the candidate tree containing the
telemetry implementation and its deterministic host regression. Documentation
commits made after that run require a final PR-tree workflow result before merge.
A merge commit must not be described as independently tested unless GitHub runs a
separate workflow against that exact commit.

## Release-phase status

| Phase | State | Real interpretation |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, deterministic QEMU gates, CI, and dated manual evidence exist. |
| v0.2 cleanup/hardening | IN PROGRESS / CANDIDATE | Syscall ownership, VFS decoupling, fail-closed RPi behavior, process lifecycle, bounded timer callback, and first runtime timing telemetry are implemented. Work budgets and stress proof remain. |
| v0.3 storage/VFS platform | NOT STARTED as a milestone | Mount callbacks, MBR parsing, and block views exist, but there is no common path resolver, rich block metadata, or structured filesystem ABI. |
| v0.4 real FAT | NOT STARTED | FAT remains root-only 8.3 FAT32. |
| v0.5 userland runtime/widgets | NOT STARTED | No reusable heap, dynamic containers, or widget toolkit exists. |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Seven applications run, but the complete daily-use workflows tracked by issue #2 are not implemented. |
| v0.7 ext2 | NOT STARTED | No ext2 implementation exists. |
| v0.8 polish | EARLY PARTIAL | Focus, dragging, minimize/restore, damage, and a panel exist; sustained visible-session evidence does not. |
| v0.9 beta | NOT STARTED | No ABI freeze, fuzz campaign, reboot-persistence gate, or beta record exists. |
| v1.0 | NOT READY | Storage, applications, ext2, persistence, runtime bounds, and final evidence are incomplete. |

## Verification record

| Check | Evidence class | Result and scope |
|---|---|---|
| `make BOARD=qemu_virt` | BUILD-VERIFIED | Kernel and seven KLI1 applications build for QEMU. |
| `make BOARD=qemu_virt size` | BUILD-VERIFIED | Kernel preserves `.data == 0` and the 108000-byte binary limit with telemetry present. |
| `make -C tests test` | HOST-VERIFIED | Native kernel, memory, VFS, FAT32, GUI, parser, driver, and ABI tests pass. |
| `bash tests/run_runtime_service_test.sh` | HOST-VERIFIED | Coalescing, EOI order, requeue preservation, deterministic timing, last/max/total duration, interval overrun counting, snapshot state, and reset behavior pass. |
| `bash tests/run_process_parent_wait_test.sh` | HOST-VERIFIED | Parent-owned zombies remain observable; foreign waits fail; abandoned zombies are reclaimable. |
| `bash tests/run_vfs_process_fd_test.sh` | HOST-VERIFIED | Descriptors are process-local and close on process exit. |
| `bash tests/run_user_copy_permissions_test.sh` | HOST-VERIFIED | Writable destinations succeed; read-only and mixed ranges fail before partial output. |
| `bash tests/run_kli1_contract_test.sh` | HOST-VERIFIED | All seven applications have empty mutable `.data`/`.bss`; synthetic violations fail. |
| `make stack-check` | HOST-VERIFIED | Recorded maximum remains 368 bytes in Editor against a 3072-byte limit. |
| RPi4 build/probe gates | BUILD/HOST-VERIFIED | Normal RPi4 and diagnostic images build; unsupported normal capabilities remain fail closed. |
| `make qemu-fs-test` | QEMU-VERIFIED | Storage initialization, FAT32 mount, shell bytes, edit file, and FAT application image markers appear. |
| `bash tools/qemu_usercopy_test.sh` | QEMU-VERIFIED | Invalid RX-output probes are rejected and the desktop continues. |
| `bash tools/qemu_focus_test.sh` | QEMU-VERIFIED | Six focus transitions occur across six created application windows. |
| `bash tools/qemu_marker_test.sh all` | QEMU-VERIFIED | Framebuffer, USB, and DHCP marker gates pass on the verified baseline. |
| `bash tools/qemu_fb_fat_test.sh` | QEMU-VERIFIED | FAT32, display, and panel readiness appear in one boot. |
| `make qemu-fb-visible` | MANUAL-VERIFIED, dated | Rocco verified create/edit/save/rename/reopen/delete on 2026-07-17. No newer visible pass is recorded. |
| Physical Raspberry Pi boot | UNVERIFIED | No repeatable physical boot, timer, storage, framebuffer, or input evidence exists. |

## Runtime architecture

### EL0 scheduling

EL0 processes are preemptive. IRQ entry saves the interrupted process frame and
the dispatcher may select another ready process before exception return.

The process model remains fixed-capacity:

- 16 process slots;
- eight tracked user regions per process;
- private TTBR0 roots;
- parent PID and zombie state;
- non-blocking wait for a zombie child;
- automatic reclamation only for kernel-owned or orphaned zombies.

### EL1 execution modes

EL1 helper threads are cooperative and switch only at explicit yield/exit
boundaries. The deferred runtime service is a separate execution mode:

```text
timer IRQ callback
  -> tick accounting and CNTP_CVAL rearm
  -> publish RUNTIME_WORK_PERIODIC
  -> scheduler accounting
  -> board_irq_end()
  -> runtime_service_run_pending()
       -> read generic counter
       -> poll input/devices, route GUI, redraw, poll network
       -> read generic counter and update telemetry
  -> process dispatch
  -> eret
```

EOI releases the interrupt controller but does not leave the exception. During
the service pass:

- execution remains in EL1;
- normal IRQs remain masked;
- the 288-byte saved exception frame remains on the EL1 stack;
- EL0 remains paused;
- another normal IRQ cannot preempt the pass.

The physical timer callback is bounded. The complete exception path is measured
but not yet bounded.

### Runtime telemetry now implemented

The kernel-internal snapshot records:

- accepted and coalesced requests;
- non-empty and empty consumer invocations;
- passes that republish work;
- last, maximum, and cumulative generic-counter duration;
- passes exceeding the configured timer interval;
- counter frequency, threshold, pending bits, and last consumed bits.

The production clock uses `CNTPCT_EL0`; `CNTFRQ_EL0` provides the conversion
frequency. The initial threshold is one timer interval, approximately 10 ms at
the current 100 Hz configuration. It is an observation threshold, not the final
budget.

No syscall or user ABI exposes this structure. The pending word and telemetry
remain correct only under the current single-core, IRQ-masked, one-consumer
assumptions.

## Subsystem status

| Subsystem | Status | Verified behavior | Important limit |
|---|---|---|---|
| AArch64 boot | IMPLEMENTED; BUILD/QEMU-VERIFIED | DTB handoff, vectors, UART, PMM/VMM, desktop launch | No long-duration stress or hardware validation |
| PMM | IMPLEMENTED; HOST-VERIFIED | Fixed bitmap allocation/reserve/free/accounting | At most 128 MiB managed |
| VMM/MMU | IMPLEMENTED; HOST-VERIFIED | 4 KiB tables, W^X, user mappings, TTBR0 switching | Kernel mappings duplicated per TTBR0; no TTBR1/ASID/scoped invalidation |
| EL0 processes | IMPLEMENTED; HOST/QEMU-VERIFIED | Spawn, argv, yield, preemption, kill, exit, wait lifecycle | 16 slots and eight user regions |
| EL1 scheduler | IMPLEMENTED | Cooperative helper threads | Not a preemptive/wakeable service scheduler |
| Deferred runtime | IMPLEMENTED; HOST/QEMU-VERIFIED for integration | Post-EOI dispatch, coalescing, requeue, and duration telemetry | No event/packet/device/redraw/global budgets or sustained-load proof |
| Syscall ABI | IMPLEMENTED; HOST/QEMU-VERIFIED on selected paths | Frozen numbers, kernel temporaries, permission-aware pointers | Final copies are not fault-recoverable |
| VFS | IMPLEMENTED; HOST-VERIFIED | Four mount slots, callbacks, process-local descriptors | 24 nodes, eight FDs/process, 64-byte paths, no common resolver |
| FAT32 | IMPLEMENTED; HOST/QEMU/MANUAL-VERIFIED for narrow flow | Root 8.3 create/read/write/list/rename/delete | No directories, LFN, broad compatibility, GPT, or crash recovery |
| GUI | IMPLEMENTED; HOST/QEMU/MANUAL-VERIFIED | 16 windows, ownership, focus, dragging, minimize/restore, backing/damage/events | Redraw cost is measured only as part of the aggregate pass, not independently budgeted |
| Input | IMPLEMENTED; HOST/QEMU-VERIFIED | UART, virtio-input, and direct USB HID feed one queue | 64-event queue; all available events may be drained in one pass |
| USB | IMPLEMENTED; HOST/QEMU-VERIFIED | QEMU xHCI and directly attached keyboard/mouse HID | No hubs or general USB class framework |
| Network | IMPLEMENTED; HOST/QEMU-VERIFIED | Ethernet, ARP, IPv4, UDP, DHCP | No sockets/TCP/DNS/HTTP; receive work has no budget |
| KLI1 | IMPLEMENTED; HOST-VERIFIED | Flat images and seven applications | Mutable static `.data`/`.bss` forbidden |
| RPi4 | SCAFFOLDING; BUILD/HOST-VERIFIED | Board contract and read-only diagnostic components | Normal capabilities zero; no physical evidence |

## Application status

| Application | Current behavior | Missing for v1 |
|---|---|---|
| Panel | Launch, taskbar, focus/restore/minimize, clock | Repeated-use polish and duplicate-launch policy |
| Files | Lists `/fat`, up to eight root entries, basic 8.3 operations, opens Editor | Directories, volumes, long names, scrolling, metadata, copy/move |
| Editor | 512-byte buffer, caret/edit/newline/navigation/save | Multi-line viewport, scrolling, larger files, save-as, richer state |
| Shell | History, scrollback, diagnostics and basic file/process commands | `cp`, `mv`, `rm`, `mkdir`, `touch`, `echo`, `edit`, `open`, `df` |
| Monitor | Displays selected system/process information | Process controls and deliberate runtime telemetry ABI |
| Control | Demonstrates settings surface | Persistent configuration and observable preferences |
| Clock | Displays time/ticks | Integration and persistent visibility preference |

Issue #2 is the v0.6 useful-applications milestone. It depends on the v0.3 path
and metadata platform, v0.4 real FAT, and v0.5 shared runtime/widgets. Small
isolated usability fixes may land earlier but must not replace those foundations.

## Open risks by release impact

### Blocks formal v0.2 promotion

- **RISK-017:** runtime duration is now measured, but work remains unbounded.
- Per-class queue, event, device, packet, redraw, and budget-exhaustion metrics are absent.
- No sustained-load QEMU heartbeat test proves EL0 progress or no silent loss.
- No formal v0.2 tag/evidence record exists.

### Required before v1.0

- **RISK-013:** storage/VFS platform is too narrow.
- **RISK-014:** applications are not complete daily tools.
- No ext2 implementation exists.
- No combined files/settings reboot-persistence gate exists.
- No 30-minute stable manual desktop session is recorded.

### Ongoing hardening

- **RISK-015:** copyin/copyout is permission-aware but not fault-contained.
- TTBR1, ASIDs, and scoped TLB invalidation are absent.
- Fixed capacities and global queues limit scalability.

### Hardware track

- **RISK-007:** no physical Raspberry Pi storage or boot evidence exists.

## Explicit non-claims

ArmoniOS does not currently claim bounded worst-case interrupt-to-EL0 latency,
SMP, POSIX/libc, dynamic linking, general FAT compatibility, FAT long names or
directories, ext2, USB hubs, sockets/TCP/DNS/HTTP, audio, accelerated graphics,
complete daily-use applications, or physical Raspberry Pi support.

## Promotion gate

The one-command automated baseline is:

```sh
bash tools/verify.sh
```

Manual visible claims require:

```sh
make qemu-fb-visible
```

Record the tester, date, exact commit, workflow, result, and limitations.

## Next technically correct sequence

1. Add per-class input, device, packet, redraw, queue, and overflow metrics.
2. Use measured data to define independent per-pass and global time budgets.
3. Preserve or republish specific pending bits when a budget expires.
4. Add sustained-load QEMU tests proving EL0 heartbeat progress and explicit loss accounting.
5. Close or explicitly accept RISK-017 and perform a dated visible desktop pass.
6. Promote/tag v0.2 only after the final evidence record is complete.
7. Begin v0.3 storage/VFS platform work.

## Maintenance rule

Behavior changes must update this file, the relevant architecture/ABI document,
`TECHNICAL_RISKS.md`, `ROADMAP.md`, README, and `AGENTS.md` when their claims are
affected. Do not create a competing status document.
