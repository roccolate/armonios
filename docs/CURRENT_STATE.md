# Current State

> Operational source of truth for ArmoniOS.
>
> Documentation status rules: `DOCUMENTATION_POLICY.md`  
> Active correctness and release risks: `TECHNICAL_RISKS.md`

## Audit metadata

- **Project state:** v0.9 QEMU desktop alpha
- **Release target:** v1.0 QEMU desktop release candidate
- **Audit date:** 2026-07-13
- **Code baseline synchronized:** `main` at `bb7c2eb910dcfecc87990cea7ad3afcdb08ada8b`
- **Audit method:** repository-wide static inspection plus existing local verification records in issue #1
- **Not performed after the latest test-infrastructure merge:** local compilation, real QEMU execution, visible desktop verification, or physical Raspberry Pi testing
- **Code baseline synchronized:** `main` at `662f3ee4032ef09dac6872e1a06e8c60c3ac7611`
- **Audit method:** repository-wide static inspection plus existing local verification records in issue #1
- **Not performed after the visible-recovery merge:** local compilation, QEMU execution, or physical Raspberry Pi testing

The v0.9 label means that a usable desktop path exists in QEMU. It does **not** mean the kernel is hardened, the visible FAT workflow is complete, or Raspberry Pi support exists.

## Evidence terminology

This document uses the labels defined in `DOCUMENTATION_POLICY.md`:

- `IMPLEMENTED`
- `HOST-VERIFIED`
- `BUILD-VERIFIED`
- `QEMU-VERIFIED`
- `MANUAL-VERIFIED`
- `UNVERIFIED`
- `KNOWN-BROKEN`
- `PLANNED`

Code or test-tool presence is never upgraded to a runtime claim without matching evidence.

## Current verification record

The latest local verification recorded by Roque in issue #1 confirms an earlier baseline:

| Check | Status | Evidence |
|---|---|---|
| `make` | BUILD-VERIFIED on the earlier audited baseline | Completed locally after installing the AArch64 toolchain. Current `main` has not been rebuilt yet. |
| `make size` | BUILD-VERIFIED on the earlier audited baseline | `kernel.bin: 92696 bytes (limit: 100000)`. Current size is pending. |
| `make -C tests test` | HOST-VERIFIED on the earlier audited baseline | Reported `ALL TESTS PASSED (0)`. Current suite is pending. |
| `make stack-check` | HOST-VERIFIED on the earlier audited baseline | Maximum reported stack use: 368 bytes in `editor` with a 3072-byte limit. Current result is pending. |
| `make qemu-fs-test` | QEMU-VERIFIED on the earlier audited baseline | Passed after serial logging moved to QEMU's `-serial file:` path. Current rerun is pending. |
| `bash tools/verify_qemu.sh` | IMPLEMENTED; UNVERIFIED | Deterministic framebuffer, USB, and DHCP marker runners are in `main`; no real QEMU result has been recorded. |
| `make qemu-fb-visible` | IMPLEMENTED; UNVERIFIED after recovery change | The target attaches FAT32 storage and new userland windows request focus. The full workflow has not been rerun. |
| `make BOARD=rpi4` | KNOWN-BROKEN | Static inspection shows the board backend does not satisfy the full interface used by generic kernel code. |
| Physical Raspberry Pi 4 boot | PLANNED | No hardware boot claim. |

The historical commands above were not rerun after the recovery and marker-runner commits. Their previous scope is preserved explicitly instead of being promoted to current `main`.
| `make` | BUILD-VERIFIED on the earlier audited baseline | Completed locally after installing the AArch64 toolchain. The post-recovery commit has not been rebuilt yet. |
| `make size` | BUILD-VERIFIED on the earlier audited baseline | `kernel.bin: 92696 bytes (limit: 100000)`. The post-recovery size is pending. |
| `make -C tests test` | HOST-VERIFIED on the earlier audited baseline | Reported `ALL TESTS PASSED (0)`. The post-recovery suite is pending. |
| `make stack-check` | HOST-VERIFIED on the earlier audited baseline | Maximum reported stack use: 368 bytes in `editor` with a 3072-byte limit. The post-recovery result is pending. |
| `make qemu-fs-test` | QEMU-VERIFIED on the earlier audited baseline | Passed after serial logging moved to QEMU's `-serial file:` path. The post-recovery rerun is pending. |
| `timeout 25s make qemu-fb` | UNVERIFIED | No current deterministic pass record in the repository. |
| `timeout 25s make qemu-usb` | UNVERIFIED | No current deterministic pass record in the repository. |
| `timeout 25s make qemu-net` | UNVERIFIED | No current deterministic pass record in the repository. |
| `make qemu-fb-visible` | IMPLEMENTED; UNVERIFIED after recovery change | The target now attaches the generated FAT32 image and new userland windows request focus. The full workflow has not been rerun. |
| `make BOARD=rpi4` | KNOWN-BROKEN | Static inspection shows the board backend does not satisfy the full interface used by generic kernel code. |
| Physical Raspberry Pi 4 boot | PLANNED | No hardware boot claim. |

The historical commands above were not rerun after commit `662f3ee4032ef09dac6872e1a06e8c60c3ac7611`. Their previous scope is preserved explicitly instead of being promoted to the new baseline.

## Subsystem status

| Subsystem | Status | Current evidence | Important limitation |
|---|---|---|---|
| AArch64 QEMU boot | BUILD-VERIFIED; QEMU-VERIFIED on earlier baseline | Boot code, DTB parsing, UART markers, and the storage smoke path | Current `main` has not been rebuilt or rerun. |
| EL0 processes | IMPLEMENTED; HOST-VERIFIED on earlier baseline | Process table, saved trap frames, per-process page tables, spawn/wait/kill/exit tests | User-output pointer permissions are not enforced separately from range membership on `main`; draft PR #16 addresses this. |
| EL0 scheduling | IMPLEMENTED; HOST-VERIFIED on earlier baseline | Timer IRQ dispatch and process round-robin tests | Runtime stress/preemption coverage is limited. |
| EL1 kernel threads | IMPLEMENTED | Cooperative scheduler code | Kernel threads are not timer-preempted. |
| PMM/VMM/heap | IMPLEMENTED; HOST-VERIFIED on earlier baseline | Allocation, mapping, rollback, cleanup, and heap tests | PMM manages at most 128 MiB; kernel RAM mappings are RWX identity mappings. |
| Syscall ABI | IMPLEMENTED; HOST-VERIFIED on earlier baseline | Frozen numbers and ABI tests | Pointer checks on `main` validate registered ranges, not read/write permissions. |
| VFS | IMPLEMENTED; HOST-VERIFIED on earlier baseline | bootfs/tmpfs/FAT dispatch and descriptor tests | Eight VFS file descriptors are global on `main`; draft PR #14 adds process-local namespaces and cleanup. |
| FAT32 | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on earlier storage smoke path | Root 8.3 create/read/write/rename/delete/list plus earlier QEMU mount markers | No subdirectories, long names, general FAT compatibility, or completed visible workflow claim. |
| GUI compositor | IMPLEMENTED; HOST-VERIFIED; limited MANUAL-VERIFIED on earlier baseline | Windows, ownership, focus, drag, backing buffers, damage, and events | New-window focus recovery is implemented but not manually rerun. |
| Desktop apps | IMPLEMENTED; BUILD-VERIFIED on earlier baseline; limited MANUAL-VERIFIED | Six apps built previously; panel and `files` were observed visibly | The complete files/editor/FAT workflow is unverified after recovery. |
| virtio block | IMPLEMENTED; QEMU-VERIFIED on earlier storage smoke path | FAT storage smoke target and visible-target wiring | Visible desktop attachment is implemented but unverified. |
| virtio GPU | IMPLEMENTED; earlier MANUAL-VERIFIED; deterministic runner IMPLEMENTED | `tools/qemu_marker_test.sh fb` asserts GPU/window and panel-ready markers | No real run of the new marker gate is recorded. |
| virtio input | IMPLEMENTED; HOST-VERIFIED on earlier baseline | Parser/driver tests | Visible input behavior still requires manual workflow verification. |
| USB xHCI/HID | IMPLEMENTED; HOST-VERIFIED; deterministic runner IMPLEMENTED | USB marker runner requires controller, enumeration, and two HID devices | No real run of the new marker gate; no hub support claim. |
| virtio network/DHCP | IMPLEMENTED; HOST-VERIFIED; deterministic runner IMPLEMENTED | Network runner requires initialization and DHCP ACK | Real end-to-end QEMU DHCP remains `UNVERIFIED`; no sockets, TCP, or HTTP. |
| KLI1 application images | IMPLEMENTED; HOST-VERIFIED on earlier baseline | Image layout and shipping blob tests | Mutable `.data`/`.bss` contract is undefined. |
| Raspberry Pi 4 board layer | IMPLEMENTED; KNOWN-BROKEN; UNVERIFIED | Initial board, linker, mailbox, and eMMC files exist | Target is not build- or boot-verified; contract is incomplete and eMMC is experimental. |
| AArch64 QEMU boot | BUILD-VERIFIED; QEMU-VERIFIED on earlier baseline | Boot code, DTB parsing, UART markers, and the storage smoke path | The full desktop/runtime matrix and current recovery commit are not automated or rerun. |
| EL0 processes | IMPLEMENTED; HOST-VERIFIED | Process table, saved trap frames, per-process page tables, spawn/wait/kill/exit tests | User-output pointer permissions are not enforced separately from range membership on `main`. |
| EL0 scheduling | IMPLEMENTED; HOST-VERIFIED | Timer IRQ dispatch and process round-robin tests | Runtime stress/preemption coverage is limited. |
| EL1 kernel threads | IMPLEMENTED | Cooperative scheduler code | Kernel threads are not timer-preempted. |
| PMM/VMM/heap | IMPLEMENTED; HOST-VERIFIED | Allocation, mapping, rollback, cleanup, and heap tests | PMM manages at most 128 MiB; kernel RAM mappings are RWX identity mappings. |
| Syscall ABI | IMPLEMENTED; HOST-VERIFIED | Frozen numbers and ABI tests | Pointer checks on `main` validate registered ranges, not read/write permissions. |
| VFS | IMPLEMENTED; HOST-VERIFIED | bootfs/tmpfs/FAT dispatch and descriptor tests | Eight VFS file descriptors are global to the kernel, not process-owned. |
| FAT32 | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on storage smoke path | Root 8.3 create/read/write/rename/delete/list plus QEMU mount markers | No subdirectories, long names, general FAT compatibility, or completed visible workflow claim. |
| GUI compositor | IMPLEMENTED; HOST-VERIFIED; limited MANUAL-VERIFIED | Windows, ownership, focus, drag, backing buffers, damage, events, and an earlier visible observation | Common userland window creation now requests focus, but the post-change files-to-editor flow is UNVERIFIED. |
| Desktop apps | IMPLEMENTED; BUILD-VERIFIED on earlier baseline; limited MANUAL-VERIFIED | Six apps built previously; panel and `files` were observed visibly | The complete files/editor/FAT workflow is not verified after the recovery change. |
| virtio block | IMPLEMENTED; QEMU-VERIFIED on storage smoke path | FAT storage smoke target and visible-target wiring | Visible desktop attachment is IMPLEMENTED but its runtime result is UNVERIFIED. |
| virtio GPU | IMPLEMENTED; MANUAL-VERIFIED on earlier baseline | A visible desktop frame and windows were observed | No deterministic framebuffer completion test. |
| virtio input | IMPLEMENTED; HOST-VERIFIED | Parser/driver tests | QEMU runtime result is not recorded as a deterministic gate. |
| USB xHCI/HID | IMPLEMENTED; HOST-VERIFIED | PCI, USB, HID parsing tests | Basic directly attached boot devices only; no hub support claim. |
| virtio network/DHCP | IMPLEMENTED; HOST-VERIFIED | DHCP option parser tests and minimal stack code | End-to-end QEMU DHCP remains `UNVERIFIED`; no sockets, TCP, or HTTP. |
| KLI1 application images | IMPLEMENTED; HOST-VERIFIED | Image layout and shipping blob tests | Mutable `.data`/`.bss` contract is undefined. |
| Raspberry Pi 4 board layer | IMPLEMENTED; KNOWN-BROKEN; UNVERIFIED | Initial board, linker, mailbox, and eMMC files exist | Board target is not build- or boot-verified; contract is incomplete and eMMC is experimental. |

## Confirmed implementation facts

The current QEMU codebase includes:

- AArch64 EL1 kernel entry with DTB handoff;
- physical and virtual memory managers;
- EL0 processes with private image, stack, anonymous mappings, and page-table roots;
- preemptive EL0 dispatch on IRQ and voluntary yield;
- cooperative EL1 helper threads;
- KLI1 freestanding application images;
- a kernel-owned GUI compositor;
- panel, shell, editor, files, monitor, and clock applications;
- bootfs, tmpfs, and a small FAT32 root filesystem bridge;
- virtio block, GPU, input, and network paths for QEMU;
- PCI/xHCI and boot-protocol HID parsing;
- a broad native host test suite;
- a visible QEMU target wired to the generated FAT32 virtio block image;
- a common userland window-create wrapper that requests focus after successful creation;
- `tools/verify.sh` as the one-command local baseline;
- deterministic serial-marker tools for framebuffer, USB, and DHCP QEMU paths.
- `tools/verify.sh` as the one-command local automated baseline.

These implementation facts do not override the limitations in the subsystem table or active risk register.

## v1.0 blockers

The following items block a trustworthy v1.0 release-candidate claim:

1. **RISK-001:** merge and verify permission-aware kernel/user copies.
2. **RISK-002:** merge and verify process-local file descriptors and exit cleanup.
3. **RISK-003:** verify the visible target's FAT32 attachment through the complete workflow.
4. **RISK-004:** verify new-window focus in the files-to-editor workflow.
5. **RISK-005:** execute the deterministic framebuffer, USB, and DHCP marker gates on real QEMU and record their logs.
6. Complete the visible create/edit/save/rename/reopen/delete FAT workflow.
7. Resolve the GitHub Actions pre-step infrastructure failure tracked in issue #12.
8. Reconcile release documentation after the verified blockers are closed.
1. **RISK-001:** distinguish readable and writable user regions at the syscall boundary.
2. **RISK-002:** make file descriptors process-owned and reclaim them on exit.
3. **RISK-003:** verify the visible target's new FAT32 attachment through the complete workflow.
4. **RISK-004:** verify the new-window focus behavior in the files-to-editor workflow.
5. **RISK-005:** make framebuffer, USB, and network gates deterministic rather than timeout-only launches.
6. Complete the visible create/edit/save/rename/reopen/delete FAT workflow.
7. Resolve the GitHub Actions pre-step infrastructure failure tracked in issue #12.
8. Reconcile README, roadmap, ABI documents, and this file after verified blockers are closed.

## Explicit non-claims

ArmoniOS does not currently claim:

- production security or hardened process isolation;
- permission-aware user-output enforcement on `main`;
- per-process file descriptor isolation on `main`;
- POSIX or libc compatibility;
- general FAT32 compatibility;
- a verified visible FAT/editor workflow on the current commit;
- a real pass of the new framebuffer, USB, or DHCP marker gates;
- USB hubs;
- TCP, sockets, DNS queries, or HTTP applications;
- SMP or secondary-core startup;
- audio or accelerated graphics;
- Raspberry Pi 4 or Raspberry Pi 5 support;
- a valid Raspberry Pi SD/eMMC storage driver;
- a stable package or dynamic linking format.

## Current release gates

The one-command local baseline is:
The one-command local automated baseline is:

```sh
bash tools/verify.sh
```

It runs:

```sh
bash tools/verify.sh
```

It currently runs build, size, host tests, stack checking, and the FAT32 storage smoke test.

The deterministic QEMU subsystem runner is now available:

```sh
bash tools/verify_qemu.sh
```

It captures separate serial logs and requires explicit completion markers for framebuffer, USB, and DHCP. Until a named tester runs it on an exact commit, its subsystem results remain `UNVERIFIED`.

Manual visible verification uses:
Manual visible verification uses the target that now includes GPU, input, and FAT storage:

```sh
make qemu-fb-visible
```

The target wiring is implemented. The create/edit/save/rename/reopen/delete workflow remains `UNVERIFIED` until a named tester records the result on the exact commit.
The target wiring is implemented. The documented create/edit/save/rename/reopen/delete workflow remains `UNVERIFIED` until a named tester records the result on the exact commit.

## Maintenance rule

Update this document only from evidence:

- record the exact branch or commit;
- record commands actually run;
- name the person who performed manual or hardware checks;
- attach or reference generated serial logs;
- leave unrun checks as `UNVERIFIED`;
- update `TECHNICAL_RISKS.md` in the same change when a blocker changes state.

Do not infer release readiness from a merged test tool, a timeout, a closed historical review, or a code comment.
