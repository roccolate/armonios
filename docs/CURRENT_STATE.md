# Current State

> Operational source of truth for ArmoniOS.
>
> Evidence terminology: `DOCUMENTATION_POLICY.md`
> Active correctness and release risks: `TECHNICAL_RISKS.md`
> Future intent and milestone ordering: `ROADMAP.md`

## Executive classification

ArmoniOS is currently a **v0.1 QEMU desktop baseline** and an active **v0.2
cleanup/runtime-hardening candidate**.

That classification means:

- the QEMU `virt` kernel, desktop, storage demo, applications, and verification
  matrix are real and reproducible;
- most original v0.2 cleanup goals have landed;
- the project has not yet promoted a formal v0.2 release because deferred-runtime
  latency remains unbounded and no v0.2 release tag/evidence record exists;
- v0.3 storage-platform work, general FAT, ext2, shared widgets, and complete
  desktop applications remain future work;
- Raspberry Pi remains a separate unverified hardware track.

ArmoniOS should be described publicly as:

> A compact AArch64 QEMU desktop alpha with freestanding EL0 applications, a
> kernel compositor, a narrow writable FAT32 workflow, and a broad automated
> verification baseline.

It should not be described as a production OS, a general FAT implementation, a
POSIX system, or a Raspberry Pi operating system.

## Audit metadata

- **Audit date:** 2026-07-21
- **Primary verified platform:** QEMU `virt`, Cortex-A72 CPU model
- **Audited runtime merge:** `ea13f0e4827a98d40f5cac0f3d21527aa4a5d11c`
- **Validated PR head:** `fedb07063c39f81297f36592d56bcb37a3844560`
- **Merged PR:** #41, deferred runtime service
- **Documentation sync:** documentation-only branch based on `ea13f0e`
- **Latest hosted evidence:**
  - `Verify ArmoniOS` run `29824050151`: success
  - `CI - Tests` run `29824050165`: success

The workflows ran against the PR merge tree containing `fedb070`. The resulting
runtime code was merged into `main` as `ea13f0e`. GitHub did not run a separate
workflow against the final merge commit, so the merge must not be described as a
new independent test execution.

## Release-phase status

| Phase | State | Real interpretation |
|---|---|---|
| v0.1 QEMU baseline | COMPLETE | Boot, desktop, narrow FAT workflow, core isolation fixes, deterministic QEMU gates, CI, and dated manual evidence exist. |
| v0.2 cleanup/hardening | IN PROGRESS / CANDIDATE | Kernel-owned syscall buffers, VFS decoupling, fail-closed RPi behavior, process lifecycle, and bounded timer callback are implemented. Runtime-service budgets and formal promotion remain. |
| v0.3 storage/VFS platform | NOT STARTED as a milestone | Pieces exist—mount callbacks, MBR parser, block views—but no common path resolver, rich block metadata, or structured filesystem ABI. |
| v0.4 real FAT | NOT STARTED | Current implementation remains FAT32 root-only 8.3. |
| v0.5 userland runtime/widgets | NOT STARTED | `SYS_MMAP` is used directly; no reusable heap, dynamic containers, or widget toolkit exists. |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Seven applications run, but Files, Editor, Shell, Control, and Monitor do not satisfy daily-use workflows. |
| v0.7 ext2 | NOT STARTED | No ext2 implementation exists. |
| v0.8 polish | EARLY PARTIAL | Focus, dragging, minimize/restore, damage, and a panel exist; sustained manual-session evidence and broader polish do not. |
| v0.9 beta | NOT STARTED | No ABI freeze, fuzz campaign, persistence reboot gate, or beta evidence record. |
| v1.0 | NOT READY | Storage, applications, ext2, persistence, runtime bounds, and final evidence are incomplete. |

## Verification record

| Check | Evidence class | Result and scope |
|---|---|---|
| `make BOARD=qemu_virt` | BUILD-VERIFIED | Kernel and seven KLI1 applications build for QEMU. |
| `make BOARD=qemu_virt size` | BUILD-VERIFIED | Kernel preserves `.data == 0` and the 108000-byte binary limit. |
| `make -C tests test` | HOST-VERIFIED | Native kernel, memory, VFS, FAT32, GUI, parser, driver, and ABI tests pass. |
| `bash tests/run_runtime_service_test.sh` | HOST-VERIFIED | Pending work coalesces, backend requeue survives, work runs after EOI, and the timer source contains no direct runtime backend calls. |
| `bash tests/run_process_parent_wait_test.sh` | HOST-VERIFIED | Parent-owned zombies remain observable; foreign waits fail; abandoned zombies are reclaimable. |
| `bash tests/run_vfs_process_fd_test.sh` | HOST-VERIFIED | Descriptors are process-local and are closed on process exit. |
| `bash tests/run_user_copy_permissions_test.sh` | HOST-VERIFIED | Writable destinations succeed; read-only destinations return `ERR_PERM`; mixed ranges fail before partial output. |
| `bash tests/run_kli1_contract_test.sh` | HOST-VERIFIED | All seven shipping applications have empty mutable `.data`/`.bss`; synthetic violations fail. |
| `make stack-check` | HOST-VERIFIED | Recorded maximum is 368 bytes in Editor against a 3072-byte limit. |
| `bash tests/run_board_build_test.sh` | BUILD/HOST-VERIFIED | Normal RPi4 and diagnostic paths build with unsupported capabilities failing closed. |
| RPi4 EMMC2/MBR/block-view gates | HOST-VERIFIED | Controller telemetry, primary-MBR FAT32 discovery, and bounded partition views are tested without claiming physical media behavior. |
| `make qemu-fs-test` | QEMU-VERIFIED | Storage initialization, FAT32 mount, root mount, shell bytes, edit file, and FAT application image markers appear. |
| `bash tools/qemu_usercopy_test.sh` | QEMU-VERIFIED | Seven invalid RX-output probes are rejected and the desktop continues to panel/clock startup. |
| `bash tools/qemu_focus_test.sh` | QEMU-VERIFIED | Six focus transitions occur across six created application windows. |
| `bash tools/qemu_marker_test.sh all` | QEMU-VERIFIED | Framebuffer/window, USB controller/enumeration/two HID devices, and DHCP lease markers pass. |
| `bash tools/qemu_fb_fat_test.sh` | QEMU-VERIFIED | FAT32, display, and panel readiness appear in one visible-target boot configuration. |
| GitHub Actions | CI-VERIFIED | Runs `29824050151` and `29824050165` completed successfully for the validated PR tree. |
| `make qemu-fb-visible` workflow | MANUAL-VERIFIED, dated | Rocco verified create/edit/save/rename/reopen/delete on 2026-07-17. No newer manual desktop pass is recorded. |
| Physical Raspberry Pi boot | UNVERIFIED | No repeatable physical boot, timer, storage, framebuffer, or input evidence exists. |

## Runtime architecture: exact current behavior

### EL0 scheduling

EL0 processes are preemptive. IRQ entry saves the process trap frame, and the
process dispatcher may select another ready process before exception return.

The process model is intentionally fixed-capacity:

- 16 process slots;
- eight tracked user regions per process;
- private TTBR0 roots;
- parent PID and zombie exit state;
- non-blocking `sys_wait` for a zombie child;
- automatic reclamation only for kernel-owned or orphaned zombies.

### EL1 work

EL1 helper threads are cooperative and change only at explicit yield/exit
boundaries. They are not timer-preempted.

Timer-originated device, GUI, and network work uses a separate post-EOI runtime
service:

```text
timer IRQ callback
  -> tick accounting
  -> CNTP_CVAL rearm
  -> publish RUNTIME_WORK_PERIODIC
  -> scheduler accounting
  -> board_irq_end()
  -> runtime_service_run_pending()
  -> EL0 process dispatch
  -> eret
```

The timer callback itself is bounded. The complete exception path is not yet
bounded because the runtime service can drain all queued input, redraw, and poll
network/device paths in one pass.

EOI only releases the interrupt controller. During the runtime-service pass:

- execution remains in EL1 exception context;
- IRQs remain masked under the current vector entry;
- the saved exception frame remains on the EL1 stack;
- EL0 remains paused;
- another normal IRQ cannot preempt the service.

The pending mask is correct only under the current single-core, single-consumer
model. `volatile` storage is not an SMP-safe atomic work queue.

## Subsystem status

| Subsystem | Status | Verified behavior | Important limit |
|---|---|---|---|
| AArch64 boot | IMPLEMENTED; BUILD/QEMU-VERIFIED | DTB handoff, exception vectors, UART, PMM/VMM initialization, desktop launch | No long-duration boot/runtime stress or hardware validation |
| PMM | IMPLEMENTED; HOST-VERIFIED | Fixed bitmap allocation, reserve, free, accounting | Manages at most 128 MiB |
| VMM/MMU | IMPLEMENTED; HOST-VERIFIED | 4 KiB stage-1 tables, kernel W^X, user mappings, TTBR0 switching | Kernel mappings duplicated in each TTBR0; no TTBR1, ASIDs, or scoped TLB invalidation |
| EL0 processes | IMPLEMENTED; HOST/QEMU-VERIFIED | Spawn, argv, yield, preemption, kill, exit, parent/wait lifecycle | 16 slots; eight user regions; fixed image/stack layout |
| EL1 scheduler | IMPLEMENTED | Cooperative helper threads | Not preemptive and not integrated as a wakeable service beside active EL0 |
| Deferred runtime | IMPLEMENTED; HOST/QEMU-VERIFIED for integration | Timer callback publishes; backend executes after EOI; coalescing and requeue tested | No duration metric, event/packet/redraw budget, starvation proof, or nested IRQ service |
| Syscall ABI | IMPLEMENTED; HOST/QEMU-VERIFIED on selected paths | Frozen numbers, kernel-owned temporaries, permission-aware pointers | Non-POSIX; final copies are not exception-recoverable |
| VFS | IMPLEMENTED; HOST-VERIFIED | Static nodes, four mount slots, callback dispatch, process-local descriptors | 24 nodes, eight FDs/process, 64-byte paths, no common resolver or structured metadata ABI |
| FAT32 | IMPLEMENTED; HOST/QEMU/MANUAL-VERIFIED for narrow flow | Root 8.3 create/read/write/list/rename/delete, cluster growth, dynamic nodes | No subdirectories, LFN, broad compatibility, journaling, GPT, or crash recovery |
| GUI compositor | IMPLEMENTED; HOST/QEMU/MANUAL-VERIFIED | 16 windows, ownership, focus, dragging, minimize/restore, backing, damage, events | No shared widget toolkit; redraw work is not budgeted |
| Input | IMPLEMENTED; HOST/QEMU-VERIFIED | UART, virtio-input, and direct USB HID feed one queue | 64-event shared producer queue; runtime drains without a per-pass budget |
| USB | IMPLEMENTED; HOST/QEMU-VERIFIED | QEMU xHCI and directly attached keyboard/mouse HID | No hub support or general USB class model |
| Network | IMPLEMENTED; HOST/QEMU-VERIFIED | Ethernet, ARP, IPv4, UDP, DHCP lease | No app socket ABI, TCP, DNS query API, or HTTP client; receive budget unmeasured |
| KLI1 | IMPLEMENTED; HOST-VERIFIED | Flat header/text/rodata image, seven shipping applications | Mutable static `.data` and `.bss` forbidden; no dynamic linker or stable package format |
| RPi4 board | IMPLEMENTED scaffolding; BUILD/HOST-VERIFIED | Board contract, SDHCI core, mailbox clock query, read-only diagnostic path | Normal capabilities remain zero; no physical evidence |

## Application status

| Application | Current useful behavior | Missing for v1 |
|---|---|---|
| Panel | Launches apps, tracks windows, focuses/restores/minimizes, shows clock/taskbar | Repeated-use polish, duplicate-launch policy, stronger status behavior |
| Files | Lists `/fat`, selects up to eight root entries, creates/renames/deletes valid 8.3 files, opens Editor | Directories, volumes, long names, scrolling, metadata, copy/move, general paths |
| Editor | Loads/saves a file, 512-byte mutable buffer, caret, insert/delete/newline, line navigation | Multi-line viewport, scrolling, larger/chunked files, truncate/save-as, selection, clearer dirty state |
| Shell | History, scrollback, `pwd`, `cd`, `ls`, `cat`, `run`, `kill last`, `mem`, `ps`, `ticks` | `cp`, `mv`, `rm`, `mkdir`, `touch`, `echo`, `edit`, `open`, `df`, stronger parser/output |
| Monitor | Displays selected system/process information | Selection, refresh usability, process control, richer metrics |
| Control | Demonstrates settings/control surface | Persistent configuration and multiple observable settings |
| Clock | Displays time/ticks in a small window | Integration polish and persistent panel/display preference |

The recorded “single visible line” Editor limitation is structural: the current
renderer intentionally draws only the line containing the caret. It is not merely
an unconfirmed graphical glitch.

## Open risks by release impact

### Blocks formal v0.2 promotion

- **RISK-017:** runtime-service execution is unmeasured and unbounded per pass.
- A formal v0.2 tag/release evidence record has not been created.

### Required before v1.0

- **RISK-013:** storage/VFS platform is too narrow.
- **RISK-014:** applications are not complete daily tools.
- No ext2 implementation exists.
- No reboot-persistence QEMU workflow proves files and settings together.
- No 30-minute stable manual desktop session is recorded.

### Ongoing hardening

- **RISK-015:** copyin/copyout is permission-aware but not fault-contained.
- TTBR1, ASIDs, and scoped TLB invalidation are absent.
- Fixed capacities and global queues limit scalability.

### Hardware track

- **RISK-007:** no physical Raspberry Pi EMMC/SD evidence exists.
- CPU entry, secondary-core parking, timer, framebuffer, and input remain
  unverified on physical hardware.

## Explicit non-claims

ArmoniOS does not currently claim:

- production security or hardened hostile-process containment;
- bounded worst-case interrupt-to-EL0 return latency;
- SMP or secondary-core startup;
- POSIX or libc compatibility;
- dynamic linking or package management;
- general FAT12/16/32 interoperability;
- FAT long names or subdirectories;
- ext2 support;
- USB hubs or a general USB stack;
- application sockets, TCP, DNS queries, or HTTP;
- audio or accelerated graphics;
- complete daily-use desktop applications;
- physical Raspberry Pi boot, storage, display, or input support.

## Current promotion gate

The one-command baseline is:

```sh
bash tools/verify.sh
```

Use it before merging any code, build-contract, ABI, verified-status, or release
claim change. Documentation-only edits may use targeted static checks, but must
not alter evidence claims without the corresponding code-tree run.

Manual visible claims require:

```sh
make qemu-fb-visible
```

Record the tester, date, commit, workflow, observed result, and limitations.

## Next technically correct sequence

1. Instrument runtime-service duration and work high-water marks.
2. Add per-pass input, device, network, and redraw budgets.
3. Preserve pending work when a budget is exhausted.
4. Add sustained-load QEMU tests proving EL0 heartbeat progress and no event loss.
5. Promote and tag v0.2 only after the runtime risk is closed or explicitly accepted.
6. Begin v0.3 with block-device metadata, common path resolution, structured
   filesystem operations, and ABI design.
7. Build real FAT and then the userland runtime/app work on that platform.

## Maintenance rule

Whenever code changes one of these claims, update this file, the relevant
architecture/ABI document, `TECHNICAL_RISKS.md`, `ROADMAP.md`, README, and
`AGENTS.md` in the same pull request. Do not add a second status document.
