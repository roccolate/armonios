# Current State

> Operational source of truth for ArmoniOS.
>
> Documentation status rules: `DOCUMENTATION_POLICY.md`
> Active correctness and release risks: `TECHNICAL_RISKS.md`

## Audit metadata

- **Project state:** v0.1 QEMU desktop baseline
- **Roadmap target:** v1.0 usable QEMU desktop OS
- **Audit date:** 2026-07-21
- **Code baseline synchronized:** `main` through `4efae94`, plus PR #41 runtime code head `aec3b1f8f352968d557d0783e2fc360c982a3a11`
- **Audit method:** repository-wide static inspection plus hosted verification of PR #41 runtime code head `aec3b1f`. `Verify ArmoniOS` run `29823442266` and `CI - Tests` run `29823442301` both completed successfully, covering QEMU/RPi4 build and size contracts, zero initialized `.data`, the RPi4 EMMC2 probe package, native host tests, the deferred runtime-service regression, syscall/process regressions, stack checks, FAT32 QEMU smoke, and the complete `tools/verify.sh` matrix.

## Verification record

| Check | Status | Evidence |
|---|---|---|
| `make` | BUILD-VERIFIED | Baseline build passes for `BOARD=qemu_virt`. |
| `make size` | BUILD-VERIFIED | PR #41 runtime code head passes `.data == 0` and the unchanged 108000-byte kernel limit. |
| `make -C tests test` | HOST-VERIFIED | Native suite passes, including mapped EL0 regressions for VFS, IPC/argv, GUI outputs, information outputs, read-only destinations, and event preservation. |
| `bash tests/run_runtime_service_test.sh` | HOST-VERIFIED | Periodic requests coalesce, work requeued during a pass survives, the backend runs after EOI, and the timer source contains no direct UART/device/GUI/network runtime calls. |
| `bash tests/run_process_parent_wait_test.sh` | HOST-VERIFIED | Child zombies remain observable until their parent waits; foreign waits fail and orphaned zombies are reclaimed. |
| `bash tests/run_vfs_process_fd_test.sh` | HOST-VERIFIED | `process-local VFS descriptors` and `process exit closes VFS descriptors`. |
| `bash tests/run_user_copy_permissions_test.sh` | HOST-VERIFIED | writable copies, `ERR_PERM` on read-only destinations, mixed RW→RO atomicity. |
| `bash tests/run_kli1_contract_test.sh` | HOST-VERIFIED | Seven shipping ELFs have no `.data`/`.bss`; synthetic `.bss` regressions are rejected for every app. |
| `make stack-check` | HOST-VERIFIED | Maximum 368 bytes in `editor` with a 3072-byte limit. |
| `make qemu-fs-test` | QEMU-VERIFIED | `storage: initialized`, `FAT32: mounted`, `FAT32 root: mounted`, `FAT32 shell bytes`, `FAT32 edit file: mounted`, `storage app image: FAT32`. |
| `bash tools/qemu_usercopy_test.sh` | QEMU-VERIFIED | Seven `USERCOPY: RX output rejected` probes across distinct EL0 processes followed by `panel: ready` and `clock: starting`. Log: `build-usercopy-test/qemu-usercopy-test.log`. |
| `bash tools/qemu_focus_test.sh` | QEMU-VERIFIED | Six focus transitions across six distinct windows; every focused window has a matching `GUI: create` marker. Log: `build-focus/qemu-focus-test.log`. |
| `bash tools/qemu_marker_test.sh all` | QEMU-VERIFIED | `qemu-fb` (`display: windows`, `panel: ready`), `qemu-usb` (`USB: controller initialized`, `USB: enumeration ok`, `USB HID: 2 devices`), `qemu-net` (`network: initialized`, `[net] DHCP ack: IP=10.0.2.15`). |
| `bash tools/qemu_fb_fat_test.sh` | QEMU-VERIFIED | Visible-desktop wiring: `FAT32: mounted`, `FAT32 root: mounted`, `display: windows`, `panel: ready` in the same boot. |
| `.github/workflows/tests.yml` | CI-VERIFIED | Run `29823442301` completed `bash tools/verify.sh` for code head `aec3b1f`; run `29823442266` independently passed the build/host/FAT workflow. |
| `make qemu-fb-visible` interactive workflow | MANUAL-VERIFIED | Existing manual evidence only: rocco verified Files `/fat` listing, 8.3 create, Editor open/focus/type/Ctrl-S, close, rename, reopen with content intact, delete, refresh, and no stale titlebar artifacts on 2026-07-17. Editor appeared to show one visible text line; save/reopen persistence still passed. |
| `make BOARD=rpi4` and `make rpi4-emmc2-probe` | BUILD-VERIFIED | Normal RPi4 remains fail-closed; the read-only diagnostic probe, telemetry, primary-MBR FAT32 discovery, and bounded block-view tests pass under the 108000-byte limit. |
| Physical Raspberry Pi 4 boot | PLANNED | No hardware boot claim. |

The v0.1 label means the QEMU desktop baseline gates have evidence on the current codebase. It does **not** mean production hardening, broad FAT32 compatibility, or Raspberry Pi hardware support.

## Evidence terminology

This document uses the labels defined in `DOCUMENTATION_POLICY.md`:

- `IMPLEMENTED`
- `HOST-VERIFIED`
- `BUILD-VERIFIED`
- `QEMU-VERIFIED`
- `CI-VERIFIED`
- `MANUAL-VERIFIED`
- `UNVERIFIED`
- `KNOWN-BROKEN`
- `PLANNED`

Code or test-tool presence is never upgraded to a runtime claim without matching evidence.

## Subsystem status

| Subsystem | Status | Current evidence | Important limitation |
|---|---|---|---|
| AArch64 QEMU boot | BUILD-VERIFIED; QEMU-VERIFIED on current baseline | Boot code, DTB parsing, UART markers, and the usercopy/storage smoke paths | Full desktop/runtime stress is not automated. |
| EL0 processes | IMPLEMENTED; HOST-VERIFIED | Process table, saved trap frames, per-process page tables, parent-owned zombie/wait regression, spawn/wait/kill/exit tests, packed argv import | Process capacity remains fixed; permission-aware validation is implemented, while fault-recoverable copyin/copyout is still pending. |
| EL0 scheduling | IMPLEMENTED; HOST/QEMU-VERIFIED | Timer IRQ dispatch, process round-robin tests, deferred runtime-service host gate, and complete QEMU marker matrix | Timer hard-IRQ work is bounded, but the post-EOI service is still non-preemptible and lacks execution budgets (RISK-017). |
| EL1 kernel threads | IMPLEMENTED | Cooperative scheduler code | Kernel threads are not timer-preempted; the deferred runtime service is currently a post-EOI bottom half rather than a schedulable helper thread. |
| PMM/VMM/heap | IMPLEMENTED; HOST-VERIFIED | Allocation, mapping, rollback, cleanup, heap tests, and a zero-initialized-data size gate | PMM manages at most 128 MiB; kernel RAM mappings are W^X, and mutable non-zero defaults are established by subsystem init functions so the loadable `.data` section remains empty. |
| Syscall ABI | IMPLEMENTED; HOST-VERIFIED | Frozen numbers, ABI tests, and mapped EL0 boundary regressions | VFS, argv, IPC, GUI, and information payloads cross through kernel-owned temporaries; final copies remain ordinary non-fault-contained EL1 loads/stores. |
| VFS | IMPLEMENTED; HOST-VERIFIED | Per-process descriptors, static nodes, small generic mount table, filesystem callbacks, and `process-fd-isolation` gate | Still fixed-capacity and non-POSIX; no common path resolver or structured directory/metadata ABI. |
| FAT32 | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on storage smoke path; MANUAL-VERIFIED on visible workflow | Root 8.3 create/read/write/rename/delete/list, generic VFS mount dispatch, primary-MBR FAT32 discovery and bounded block-view tests, QEMU mount markers, and the existing 2026-07-17 visible workflow | No subdirectories, long names, GPT/extended partitions, broad FAT compatibility, or physical RPi media evidence. |
| GUI compositor | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on the focus path; MANUAL-VERIFIED on visible workflow | Windows, ownership, focus, drag, backing buffers, damage, events, deferred timer-published routing/redraw, and a visible Files-to-Editor pass | Broader visual polish remains future work; redraw cost is not yet budgeted per service pass. |
| Desktop apps | IMPLEMENTED; BUILD-VERIFIED; QEMU-VERIFIED on launch/focus markers; MANUAL-VERIFIED on FAT workflow | Seven apps built: panel, shell, editor, files, monitor, control, and clock; panel survived the usercopy probe regression; focus gate covers six user-visible app windows; rocco verified the Files/Editor/FAT workflow | Apps are useful demos, not complete daily tools. Files is limited to `/fat`, Editor appears to show one visible text line, Settings persistence is narrow, and v1 requires real Files/Editor/Shell/Settings/Monitor workflows. |
| virtio block | IMPLEMENTED; QEMU-VERIFIED on storage smoke path; MANUAL-VERIFIED on visible workflow | FAT storage smoke target, visible-target wiring, and the existing 2026-07-17 visible workflow | Manual visible evidence is separate from automated QEMU marker evidence. |
| virtio GPU | IMPLEMENTED; QEMU-VERIFIED on marker gate | `tools/qemu_marker_test.sh fb` asserts GPU/window and panel-ready markers with deferred redraw active | Visible manual workflow evidence is still separate. |
| virtio input | IMPLEMENTED; HOST/QEMU-VERIFIED | Parser/driver tests plus QEMU focus and visible-desktop marker paths; timer-originated polling is deferred past EOI | Per-pass polling and event-drain budgets remain open under RISK-017. |
| USB xHCI/HID | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on marker gate | USB marker runner requires controller, enumeration, and two HID devices while timer-originated HID polling is deferred | No hub support claim; polling budgets remain unmeasured. |
| virtio network/DHCP | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on marker gate | Network runner requires initialization and DHCP ACK with timer-originated polling deferred past EOI | No sockets, TCP, DNS, or HTTP application API; receive-work budgets remain unmeasured. |
| KLI1 application images | IMPLEMENTED; HOST-VERIFIED | Image layout and shipping blob tests | Mutable `.data`/`.bss` is explicitly forbidden by the linker script and exercised by `tests/run_kli1_contract_test.sh`. |
| Raspberry Pi 4 board layer | IMPLEMENTED; HOST/BUILD-VERIFIED; UNVERIFIED on hardware | SDHCI core, firmware clock query, broken-CD adapter, telemetry, minimal read-only probe, MBR discovery, block view, and board build gates | Normal capabilities remain zero; no physical clock/card/FAT read claim and no writes (RISK-007). |

## Confirmed implementation facts

The current QEMU codebase includes:

- AArch64 EL1 kernel entry with DTB handoff;
- physical and virtual memory managers;
- EL0 processes with private image, stack, anonymous mappings, and page-table roots;
- preemptive EL0 dispatch on IRQ and voluntary yield;
- a bounded physical timer callback that publishes one coalescible periodic-work bit;
- one post-EOI deferred runtime service for timer-originated device polling, GUI routing/redraw, and network polling, with requeue preservation tested;
- cooperative EL1 helper threads; `docs/ARCHITECTURE.md`, `docs/CURRENT_STATE.md`, and `AGENTS.md` repeat the preemptive-EL0 / cooperative-EL1 distinction, and `kernel/sched/sched.c` exposes the explicit `sched_yield` boundary;
- KLI1 freestanding application images;
- a kernel-owned GUI compositor;
- panel, shell, editor, files, monitor, control, and clock applications;
- bootfs, tmpfs, and a small FAT32 root filesystem bridge;
- virtio block, GPU, input, and network paths for QEMU;
- PCI/xHCI and boot-protocol HID parsing;
- permission-aware user-copy helpers in `kernel/syscall_helpers.c`, packed argv import, kernel-owned IPC/VFS/GUI/info buffers, and state-preserving output validation;
- per-process VFS descriptors in `kernel/vfs.c`, reclaimed centrally through `process_mark_exited`;
- W^X kernel page-table construction in `kernel/mm/vmm.c` via `vmm_map_kernel_identity()`, wired into the bootstrap PGD (`kernel.c`) and per-process PGDs (`panel_boot.c`);
- a broad native host test suite, including mapped EL0 boundary tests for VFS, IPC/argv, GUI, and system-information outputs;
- a visible QEMU target wired to the generated FAT32 virtio block image;
- a common userland window-create wrapper that requests focus after successful creation;
- a KLI1 mutable-storage contract enforced by `programs/apps/image.ld` ASSERTs and verified by `tests/run_kli1_contract_test.sh`;
- `tools/verify.sh` as the one-command local baseline, including the runtime-service, parent/wait, process-FD, user-copy, KLI1, RPi4, stack, FAT32, usercopy/focus, framebuffer/USB/network, and visible FAT+GPU gates;
- deterministic serial-marker tools for framebuffer, USB, and DHCP QEMU paths.

These implementation facts do not override the limitations in the subsystem table or active risk register.

## v0.1 baseline status

The syscall-boundary P0 risks, deterministic QEMU gate scaffold, visible-desktop FAT workflow, focus path, process lifecycle, bounded timer IRQ boundary, and CI-hosted reproducibility are closed for the v0.1 QEMU desktop baseline.

The v1.0 roadmap is intentionally broader than v0.1. Runtime-service budgeting, storage, application usefulness, ext2, and desktop polish remain future work until implemented and verified.

## Explicit non-claims

ArmoniOS does not currently claim:

- production security or hardened process isolation;
- bounded worst-case runtime-service latency under sustained input/network load;
- general FAT32 compatibility;
- USB hubs;
- TCP, sockets, DNS queries, or HTTP applications;
- ext2 support;
- general writable FAT support with long names and subdirectories;
- complete daily-use desktop applications;
- a userland heap, widget toolkit, or libc-like runtime;
- SMP or secondary-core startup;
- audio or accelerated graphics;
- verified physical Raspberry Pi 4 or Raspberry Pi 5 boot/storage support;
- writable Raspberry Pi SD/eMMC storage;
- a stable package or dynamic linking format.

## Current release gates

The one-command local baseline is:

```sh
bash tools/verify.sh
```

It currently runs build and size checks, normal and probe RPi4 builds, EMMC2 telemetry/MBR/block-view regressions, native host tests, the deferred runtime-service boundary, parent-owned wait lifecycle, process-local VFS FD isolation, mapped EL0 user-copy boundary tests, the KLI1 mutable-storage contract, stack checking, FAT32 storage smoke, usercopy/focus QEMU gates, framebuffer/USB/network marker gates, and the visible-desktop FAT+GPU wiring gate. Hosted run `29823442301` completed this matrix for runtime code head `aec3b1f`; run `29823442266` independently passed the build/host/FAT workflow.

The deterministic QEMU subsystem runner is:

```sh
bash tools/verify_qemu.sh
```

It captures separate serial logs and requires explicit completion markers for framebuffer, USB, and DHCP. The 2026-07-21 `tools/verify.sh` run includes those markers.

Manual visible verification uses the target that includes GPU, input, and FAT storage:

```sh
make qemu-fb-visible
```

The target wiring and interactive create/edit/save/rename/reopen/delete workflow were manually verified by rocco on 2026-07-17. No newer manual desktop check was performed for the 2026-07-21 automated baseline.

## Maintenance rule

Update this document only from evidence:

- record the exact branch or commit;
- record commands actually run;
- name the person who performed manual or hardware checks;
- attach or reference generated serial logs;
- leave unrun checks as `UNVERIFIED`;
- update `TECHNICAL_RISKS.md` in the same change when a blocker changes state.

Do not infer release readiness from a merged test tool, a timeout, a closed historical review, or a code comment.
