# Current State

> Operational source of truth for ArmoniOS.
>
> Documentation status rules: `DOCUMENTATION_POLICY.md`  
> Active correctness and release risks: `TECHNICAL_RISKS.md`

## Audit metadata

- **Project state:** v0.9 QEMU desktop alpha
- **Release target:** v1.0 QEMU desktop release candidate
- **Audit date:** 2026-07-13
- **Code baseline inspected:** `main` at `1fc5d53e8264a022e1ca51fc233c5839e3c0a28b`
- **Audit method:** repository-wide static inspection plus existing local verification records in issue #1
- **Not performed by this documentation audit:** local compilation, QEMU execution, or physical Raspberry Pi testing

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

Code presence is never upgraded to a runtime claim without matching evidence.

## Current verification record

The latest local verification recorded by Roque in issue #1 confirms:

| Check | Status | Evidence |
|---|---|---|
| `make` | BUILD-VERIFIED | Completed locally after installing the AArch64 toolchain. |
| `make size` | BUILD-VERIFIED | `kernel.bin: 92696 bytes (limit: 100000)`. |
| `make -C tests test` | HOST-VERIFIED | Reported `ALL TESTS PASSED (0)`. |
| `make stack-check` | HOST-VERIFIED | Maximum reported stack use: 368 bytes in `editor` with a 3072-byte limit. |
| `make qemu-fs-test` | QEMU-VERIFIED | Passed after serial logging moved to QEMU's `-serial file:` path. |
| `timeout 25s make qemu-fb` | UNVERIFIED | No current deterministic pass record in the repository. |
| `timeout 25s make qemu-usb` | UNVERIFIED | No current deterministic pass record in the repository. |
| `timeout 25s make qemu-net` | UNVERIFIED | No current deterministic pass record in the repository. |
| `make qemu-fb-visible` | PARTIAL MANUAL-VERIFIED | Desktop, panel, and `files` were observed, but the target had no FAT disk and the editor focus workflow exposed a defect. |
| `make BOARD=rpi4` | UNVERIFIED / likely KNOWN-BROKEN | The board backend does not currently satisfy the full board interface used by generic kernel code. |
| Physical Raspberry Pi 4 boot | PLANNED | No hardware boot claim. |

The commands above were not rerun as part of this documentation-only audit. Their scope and remaining gaps are preserved explicitly instead of being promoted to broader claims.

## Subsystem status

| Subsystem | Status | Current evidence | Important limitation |
|---|---|---|---|
| AArch64 QEMU boot | BUILD-VERIFIED, QEMU-VERIFIED through storage smoke path | Boot code, DTB parsing, UART markers, and `qemu-fs-test` | The full desktop/runtime matrix is not automated. |
| EL0 processes | IMPLEMENTED, HOST-VERIFIED | Process table, saved trap frames, per-process page tables, spawn/wait/kill/exit tests | User-output pointer permissions are not enforced separately from range membership. |
| EL0 scheduling | IMPLEMENTED, HOST-VERIFIED | Timer IRQ dispatch and process round-robin tests | Runtime stress/preemption coverage is limited. |
| EL1 kernel threads | IMPLEMENTED | Cooperative scheduler code | Kernel threads are not timer-preempted. |
| PMM/VMM/heap | IMPLEMENTED, HOST-VERIFIED | Allocation, mapping, rollback, cleanup, and heap tests | PMM manages at most 128 MiB; kernel RAM mappings are RWX identity mappings. |
| Syscall ABI | IMPLEMENTED, HOST-VERIFIED | Frozen numbers and ABI tests | Pointer checks validate registered ranges, not read/write permissions. |
| VFS | IMPLEMENTED, HOST-VERIFIED | bootfs/tmpfs/FAT dispatch and descriptor tests | Eight VFS file descriptors are global to the kernel, not process-owned. |
| FAT32 | IMPLEMENTED, HOST-VERIFIED, QEMU-VERIFIED for mount path | Root 8.3 create/read/write/rename/delete/list and `qemu-fs-test` | No subdirectories, long names, general FAT compatibility, or completed visible workflow claim. |
| GUI compositor | IMPLEMENTED, HOST-VERIFIED, PARTIAL MANUAL-VERIFIED | Windows, ownership, focus, drag, backing buffers, damage, events | The spawned editor does not receive initial focus in the observed files workflow. |
| Desktop apps | IMPLEMENTED, BUILD-VERIFIED, PARTIAL MANUAL-VERIFIED | panel, shell, editor, files, monitor, clock | The complete files/editor/FAT workflow is not yet verified. |
| virtio block | IMPLEMENTED, QEMU-VERIFIED | FAT storage smoke target | Not attached by the current visible desktop target. |
| virtio GPU | IMPLEMENTED, PARTIAL MANUAL-VERIFIED | Visible desktop observation | No deterministic framebuffer completion test. |
| virtio input | IMPLEMENTED, HOST-VERIFIED | Parser/driver tests | QEMU runtime result is not recorded as a deterministic gate. |
| USB xHCI/HID | IMPLEMENTED, HOST-VERIFIED | PCI, USB, HID parsing tests | Basic directly attached boot devices only; no hub support claim. |
| virtio network/DHCP | IMPLEMENTED, partial HOST-VERIFIED | DHCP option tests and minimal stack code | No sockets, TCP, HTTP, or deterministic QEMU success gate. |
| KLI1 application images | IMPLEMENTED, HOST-VERIFIED for current shipping apps | Image layout and shipping blob tests | Mutable `.data`/`.bss` contract is undefined. |
| Raspberry Pi 4 board layer | EXPERIMENTAL / KNOWN-BROKEN as a support claim | Initial board, linker, mailbox, and eMMC files exist | Not build-proven, not boot-proven, incomplete board contract, experimental eMMC code. |

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
- a broad native host test suite.

These implementation facts do not override the limitations in the subsystem table or active risk register.

## v1.0 blockers

The following items block a trustworthy v1.0 release-candidate claim:

1. **RISK-001:** distinguish readable and writable user regions at the syscall boundary.
2. **RISK-002:** make file descriptors process-owned and reclaim them on exit.
3. **RISK-003:** attach the FAT32 disk to the visible desktop target.
4. **RISK-004:** fix and test initial focus for a spawned editor window.
5. **RISK-005:** make framebuffer, USB, and network gates deterministic rather than timeout-only launches.
6. Complete the visible create/edit/save/rename/reopen/delete FAT workflow.
7. Reconcile README, roadmap, ABI documents, and this file after the blockers are closed.

## Explicit non-claims

ArmoniOS does not currently claim:

- production security or hardened process isolation;
- POSIX or libc compatibility;
- per-process file descriptor isolation;
- general FAT32 compatibility;
- USB hubs;
- TCP, sockets, DNS queries, or HTTP applications;
- SMP or secondary-core startup;
- audio or accelerated graphics;
- Raspberry Pi 4 or Raspberry Pi 5 support;
- a valid Raspberry Pi SD/eMMC storage driver;
- a stable package or dynamic linking format.

## Current release gates

The intended v1.0 gate set remains:

```sh
make
make size
make -C tests test
make stack-check
make qemu-fs-test
```

The following targets must be converted into deterministic checks or accompanied by explicit marker inspection before they count as release evidence:

```sh
timeout 25s make qemu-fb
timeout 25s make qemu-usb
timeout 25s make qemu-net
```

Manual visible verification must use a target that includes GPU, input, and FAT storage:

```sh
make qemu-fb-visible
```

The current target does not yet attach the FAT image, so the documented FAT workflow cannot pass until RISK-003 is fixed.

## Maintenance rule

Update this document only from evidence:

- record the exact branch or commit;
- record commands actually run;
- name the person who performed manual or hardware checks;
- leave unrun checks as `UNVERIFIED`;
- update `TECHNICAL_RISKS.md` in the same change when a blocker changes state.

Do not infer release readiness from a closed historical review or from code comments.
