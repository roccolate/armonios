# Current State

> Operational source of truth for ArmoniOS.
>
> Documentation status rules: `DOCUMENTATION_POLICY.md`
> Active correctness and release risks: `TECHNICAL_RISKS.md`

## Audit metadata

- **Project state:** v0.9 QEMU desktop alpha
- **Release target:** v1.0 QEMU desktop release candidate
- **Audit date:** 2026-07-16
- **Code baseline synchronized:** `main` at `4494c554df212401ffbd294d1a44b5977696fa0b`
- **Audit method:** repository-wide static inspection plus a fresh local verification run of `bash tools/verify.sh`, including the new `process-fd-isolation`, `usercopy-host`, and `usercopy-qemu` automated gates. The post-fix serial log for the usercopy QEMU gate lives at `build-usercopy-test/qemu-usercopy-test.log`.

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

The latest local verification recorded for the current `main` baseline at `4494c55`:

| Check | Status | Evidence |
|---|---|---|
| `make` | BUILD-VERIFIED | Clean `make` after the toolchain fixups in `kernel/vfs.c`, `drivers/input/input.c`, `tests/Makefile`, and `tests/run_*_test.sh`. |
| `make size` | BUILD-VERIFIED | `kernel.bin: 97344 bytes (limit: 100000)`. |
| `make -C tests test` | HOST-VERIFIED | `ALL TESTS PASSED (0)`; Unity set now includes `test_syscall_helpers_user_buffers_validate_registered_ranges` and the standalone runner covers the mixed RW→RO atomicity path. |
| `bash tests/run_vfs_process_fd_test.sh` | HOST-VERIFIED | `process-local VFS descriptors` and `process exit closes VFS descriptors` both pass. |
| `bash tests/run_user_copy_permissions_test.sh` | HOST-VERIFIED | New standalone gate: writable copies succeed, read-only destinations yield `ERR_PERM`, mixed RW→RO range is rejected atomically, missing PTEs yield `ERR_INVAL`. |
| `make stack-check` | HOST-VERIFIED | Maximum reported stack use: 368 bytes in `editor` with a 3072-byte limit. |
| `make qemu-fs-test` | QEMU-VERIFIED | Fat32 storage smoke test still passes; result captured as part of `bash tools/verify.sh`. |
| `bash tools/qemu_usercopy_test.sh` | QEMU-VERIFIED | New automated gate. On commit `4494c55` the captured `build-usercopy-test/qemu-usercopy-test.log` contains six `USERCOPY: RX output rejected` probes across distinct EL0 processes followed by `panel: ready` and `clock: starting`. |
| `bash tools/verify_qemu.sh` | IMPLEMENTED; UNVERIFIED | Deterministic framebuffer, USB, and DHCP marker runners are in `main`; no real per-marker log has been recorded. |
| `timeout 25s make qemu-fb` | UNVERIFIED | No current deterministic pass record in the repository. |
| `timeout 25s make qemu-usb` | UNVERIFIED | No current deterministic pass record in the repository. |
| `timeout 25s make qemu-net` | UNVERIFIED | No current deterministic pass record in the repository. |
| `make qemu-fb-visible` | IMPLEMENTED; UNVERIFIED after recovery change | The target attaches the FAT32 image and new userland windows request focus, but the create/edit/save/rename/reopen/delete workflow has not been rerun by a named tester. |
| `make BOARD=rpi4` | KNOWN-BROKEN | Static inspection shows the board backend does not satisfy the full interface used by generic kernel code. |
| Physical Raspberry Pi 4 boot | PLANNED | No hardware boot claim. |

## Subsystem status

| Subsystem | Status | Current evidence | Important limitation |
|---|---|---|---|
| AArch64 QEMU boot | BUILD-VERIFIED; QEMU-VERIFIED on current baseline | Boot code, DTB parsing, UART markers, and the usercopy/storage smoke paths | Full desktop/runtime matrix not automated. |
| EL0 processes | IMPLEMENTED; HOST-VERIFIED | Process table, saved trap frames, per-process page tables, spawn/wait/kill/exit tests | User-output copies are now PTE-checked via `sys_user_buf_out`; richer stress coverage is still pending. |
| EL0 scheduling | IMPLEMENTED; HOST-VERIFIED | Timer IRQ dispatch and process round-robin tests | Runtime stress/preemption coverage is limited. |
| EL1 kernel threads | IMPLEMENTED | Cooperative scheduler code | Kernel threads are not timer-preempted. |
| PMM/VMM/heap | IMPLEMENTED; HOST-VERIFIED | Allocation, mapping, rollback, cleanup, and heap tests | PMM manages at most 128 MiB; kernel RAM mappings are RWX identity mappings. |
| Syscall ABI | IMPLEMENTED; HOST-VERIFIED | Frozen numbers and ABI tests | Output copies enforce per-page write permission via PTE checks; further hardening tracked under RISK-008. |
| VFS | IMPLEMENTED; HOST-VERIFIED | Per-process descriptors, bootfs/tmpfs/FAT dispatch, and `process-fd-isolation` gate | Further hardening tracked under RISK-008. |
| FAT32 | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on storage smoke path | Root 8.3 create/read/write/rename/delete/list plus QEMU mount markers | No subdirectories, long names, general FAT compatibility, or completed visible workflow claim. |
| GUI compositor | IMPLEMENTED; HOST-VERIFIED | Windows, ownership, focus, drag, backing buffers, damage, events, and a limited visible observation | New-window focus recovery is implemented but the visible files-to-editor flow is still `UNVERIFIED`. |
| Desktop apps | IMPLEMENTED; BUILD-VERIFIED on current baseline | Six apps built; panel survived the usercopy probe regression | The complete files/editor/FAT workflow is not verified after the recovery change. |
| virtio block | IMPLEMENTED; QEMU-VERIFIED on storage smoke path | FAT storage smoke target and visible-target wiring | Visible desktop attachment is implemented but unverified end-to-end. |
| virtio GPU | IMPLEMENTED; earlier MANUAL-VERIFIED; deterministic runner IMPLEMENTED | `tools/qemu_marker_test.sh fb` asserts GPU/window and panel-ready markers | No real run of the new marker gate is recorded. |
| virtio input | IMPLEMENTED; HOST-VERIFIED | Parser/driver tests | Visible input behavior still requires manual workflow verification. |
| USB xHCI/HID | IMPLEMENTED; HOST-VERIFIED; deterministic runner IMPLEMENTED | USB marker runner requires controller, enumeration, and two HID devices | No real run of the new marker gate; no hub support claim. |
| virtio network/DHCP | IMPLEMENTED; HOST-VERIFIED; deterministic runner IMPLEMENTED | Network runner requires initialization and DHCP ACK | Real end-to-end QEMU DHCP remains `UNVERIFIED`; no sockets, TCP, or HTTP. |
| KLI1 application images | IMPLEMENTED; HOST-VERIFIED | Image layout and shipping blob tests | Mutable `.data`/`.bss` contract is undefined. |
| Raspberry Pi 4 board layer | IMPLEMENTED; KNOWN-BROKEN; UNVERIFIED | Initial board, linker, mailbox, and eMMC files exist | Target is not build- or boot-verified; contract is incomplete and eMMC is experimental. |

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
- permission-aware user-copy helpers in `kernel/syscall_helpers.c` (PTE-checked `user_buf_range`, `sys_copy_from_user`, `sys_copy_to_user`, `sys_user_copy_cstr`);
- per-process VFS descriptors in `kernel/vfs.c`, reclaimed centrally through `process_mark_exited`;
- a broad native host test suite;
- a visible QEMU target wired to the generated FAT32 virtio block image;
- a common userland window-create wrapper that requests focus after successful creation;
- `tools/verify.sh` as the one-command local baseline (now also running `tests/run_vfs_process_fd_test.sh`, `tests/run_user_copy_permissions_test.sh`, and `tools/qemu_usercopy_test.sh`);
- deterministic serial-marker tools for framebuffer, USB, and DHCP QEMU paths.

These implementation facts do not override the limitations in the subsystem table or active risk register.

## v1.0 blockers

Both syscall-boundary P0 risks are now closed. The remaining v1.0 work is verification and reproducibility:

1. **RISK-003:** verify the visible target's FAT32 attachment through the complete workflow.
2. **RISK-004:** verify new-window focus in the files-to-editor workflow.
3. **RISK-005:** execute the deterministic framebuffer, USB, and DHCP marker gates on real QEMU and record their logs. The `usercopy-qemu` gate is already part of the automated baseline; the subsystem markers still need real logs.
4. Complete the visible create/edit/save/rename/reopen/delete FAT workflow.
5. Resolve the GitHub Actions pre-step infrastructure failure tracked in issue #12.
6. Reconcile release documentation after the verified blockers are closed.

## Explicit non-claims

ArmoniOS does not currently claim:

- production security or hardened process isolation;
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

```sh
bash tools/verify.sh
```

It currently runs build, size, host tests, process-local VFS FD isolation, the standalone user-copy permissions gate, stack checking, the FAT32 storage smoke test, and the new `usercopy-qemu` QEMU gate. A clean run on commit `4494c55` captured commit and output via the script's built-in commit/date header. The QEMU serial log is at `build-usercopy-test/qemu-usercopy-test.log`.

The deterministic QEMU subsystem runner is:

```sh
bash tools/verify_qemu.sh
```

It captures separate serial logs and requires explicit completion markers for framebuffer, USB, and DHCP. Until a named tester runs it on an exact commit, its subsystem results remain `UNVERIFIED`.

Manual visible verification uses the target that now includes GPU, input, and FAT storage:

```sh
make qemu-fb-visible
```

The target wiring is implemented. The create/edit/save/rename/reopen/delete workflow remains `UNVERIFIED` until a named tester records the result on the exact commit.

## Maintenance rule

Update this document only from evidence:

- record the exact branch or commit;
- record commands actually run;
- name the person who performed manual or hardware checks;
- attach or reference generated serial logs;
- leave unrun checks as `UNVERIFIED`;
- update `TECHNICAL_RISKS.md` in the same change when a blocker changes state.

Do not infer release readiness from a merged test tool, a timeout, a closed historical review, or a code comment.
