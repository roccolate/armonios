# Current State

> Operational source of truth for ArmoniOS.
>
> Documentation status rules: `DOCUMENTATION_POLICY.md`
> Active correctness and release risks: `TECHNICAL_RISKS.md`

## Audit metadata

- **Project state:** v0.9 QEMU desktop alpha
- **Release target:** v1.0 QEMU desktop release candidate
- **Audit date:** 2026-07-16
- **Code baseline synchronized:** `main` at `9157aa2360fa346dd98e9c64ac2050f8af111ce9`
- **Audit method:** repository-wide static inspection plus a fresh local verification run of `bash tools/verify.sh`, including `process-fd-isolation`, `usercopy-host`, `usercopy-qemu`, `qemu-markers`, and `qemu-fb-fat` automated gates. Serial logs for the QEMU gates live under `build/qemu-*-test.log` and `build-usercopy-test/qemu-usercopy-test.log`.

## Verification record (commit `9157aa2`)

| Check | Status | Evidence |
|---|---|---|
| `make` | BUILD-VERIFIED | Baseline build with `kernel/vfs.c`, `drivers/input/input.c`, `tests/Makefile`, and `tests/run_*_test.sh` toolchain fixups. |
| `make size` | BUILD-VERIFIED | `kernel.bin: 97344 bytes (limit: 100000)`. |
| `make -C tests test` | HOST-VERIFIED | `ALL TESTS PASSED (0)`. |
| `bash tests/run_vfs_process_fd_test.sh` | HOST-VERIFIED | `process-local VFS descriptors` and `process exit closes VFS descriptors`. |
| `bash tests/run_user_copy_permissions_test.sh` | HOST-VERIFIED | writable copies, `ERR_PERM` on read-only destinations, mixed RW→RO atomicity. |
| `make stack-check` | HOST-VERIFIED | Maximum 368 bytes in `editor` with a 3072-byte limit. |
| `make qemu-fs-test` | QEMU-VERIFIED | `storage: initialized`, `FAT32: mounted`, `FAT32 root: mounted`, `FAT32 shell bytes`, `FAT32 edit file: mounted`, `storage app image: FAT32`. |
| `bash tools/qemu_usercopy_test.sh` | QEMU-VERIFIED | Six `USERCOPY: RX output rejected` probes across distinct EL0 processes followed by `panel: ready` and `clock: starting`. Log: `build-usercopy-test/qemu-usercopy-test.log`. |
| `bash tools/qemu_marker_test.sh all` | QEMU-VERIFIED | `qemu-fb` (`VIRTIO gpu: windows`, `panel: ready`), `qemu-usb` (`USB: controller initialized`, `USB: enumeration ok`, `USB HID: 2 devices`), `qemu-net` (`network: initialized`, `[net] DHCP ack: IP=10.0.2.15`). |
| `bash tools/qemu_fb_fat_test.sh` | QEMU-VERIFIED | Visible-desktop wiring: `FAT32: mounted`, `FAT32 root: mounted`, `VIRTIO gpu: windows`, `panel: ready` in the same boot. |
| `make qemu-fb-visible` interactive workflow | UNVERIFIED | The static wiring is provable from logs; the visible create/edit/save/rename/reopen/delete flow still needs a named human tester on a real QEMU display. |
| `make BOARD=rpi4` | KNOWN-BROKEN | The RPi4 board backend does not satisfy the full interface used by generic kernel code. |
| Physical Raspberry Pi 4 boot | PLANNED | No hardware boot claim. |

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
| `bash tests/run_kli1_contract_test.sh` | HOST-VERIFIED | Each of the six shipping apps has no `.data`/`.bss` sections in the linked ELF, and a synthetic `.bss` source is rejected by the linker `ASSERT` with the KLI1 message. |
| `make stack-check` | HOST-VERIFIED | Maximum reported stack use: 368 bytes in `editor` with a 3072-byte limit. |
| `make qemu-fs-test` | QEMU-VERIFIED | Fat32 storage smoke test still passes; result captured as part of `bash tools/verify.sh`. |
| `bash tools/qemu_usercopy_test.sh` | QEMU-VERIFIED | New automated gate. On commit `4494c55` the captured `build-usercopy-test/qemu-usercopy-test.log` contains six `USERCOPY: RX output rejected` probes across distinct EL0 processes followed by `panel: ready` and `clock: starting`. |
| `bash tools/qemu_focus_test.sh` | QEMU-VERIFIED | New automated gate. Captured `build-focus/qemu-focus-test.log` records 5 `GUI: focus` transitions across 5 distinct windows (panel auto-launches shell, editor, files, monitor, clock); every focused window has a matching `GUI: create` marker. |
| `bash tools/verify_qemu.sh` | IMPLEMENTED; UNVERIFIED | Deterministic framebuffer, USB, and DHCP marker runners are in `main`; no real per-marker log has been recorded. |
| `timeout 25s make qemu-fb` | UNVERIFIED | No current deterministic pass record in the repository. |
| `timeout 25s make qemu-usb` | UNVERIFIED | No current deterministic pass record in the repository. |
| `timeout 25s make qemu-net` | UNVERIFIED | No current deterministic pass record in the repository. |
| `make qemu-fb-visible` | IMPLEMENTED; UNVERIFIED after recovery change | The target attaches the FAT32 image and new userland windows request focus, but the create/edit/save/rename/reopen/delete workflow has not been rerun by a named tester. |
| `make BOARD=rpi4` | BUILD-VERIFIED | `tests/run_board_build_test.sh` (the `board-rpi4` gate inside `tools/verify.sh`) does a clean BOARD=rpi4 build into `build-rpi4/` and asserts the kernel size gate. The RPi4 backend exposes explicit safe-failure stubs for virtio-input; eMMC and physical serial are still pending. |
| Physical Raspberry Pi 4 boot | PLANNED | No hardware boot claim. |

## Subsystem status

| Subsystem | Status | Current evidence | Important limitation |
|---|---|---|---|
| AArch64 QEMU boot | BUILD-VERIFIED; QEMU-VERIFIED on current baseline | Boot code, DTB parsing, UART markers, and the usercopy/storage smoke paths | Full desktop/runtime matrix not automated. |
| EL0 processes | IMPLEMENTED; HOST-VERIFIED | Process table, saved trap frames, per-process page tables, spawn/wait/kill/exit tests | User-output copies are now PTE-checked via `sys_user_buf_out`; richer stress coverage is still pending. |
| EL0 scheduling | IMPLEMENTED; HOST-VERIFIED | Timer IRQ dispatch and process round-robin tests | Runtime stress/preemption coverage is limited. |
| EL1 kernel threads | IMPLEMENTED | Cooperative scheduler code | Kernel threads are not timer-preempted. |
| PMM/VMM/heap | IMPLEMENTED; HOST-VERIFIED | Allocation, mapping, rollback, cleanup, and heap tests | PMM manages at most 128 MiB; kernel RAM mappings are now W^X (RISK-008: text RX, data+bss+stack RW+NX, MMIO device+NX, remaining RAM RW+NX). |
| Syscall ABI | IMPLEMENTED; HOST-VERIFIED | Frozen numbers and ABI tests | Output copies enforce per-page write permission via PTE checks; memory hardening implemented under RISK-008. |
| VFS | IMPLEMENTED; HOST-VERIFIED | Per-process descriptors, bootfs/tmpfs/FAT dispatch, and `process-fd-isolation` gate | Memory hardening for VFS tracked under RISK-008 (now closed). |
| FAT32 | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on storage smoke path | Root 8.3 create/read/write/rename/delete/list plus QEMU mount markers | No subdirectories, long names, general FAT compatibility, or completed visible workflow claim. |
| GUI compositor | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on the focus path | Windows, ownership, focus, drag, backing buffers, damage, events, and a limited visible observation; `tools/qemu_focus_test.sh` proves the focus syscall path runs end-to-end | The visible files-to-editor workflow still needs a named human tester on a real QEMU display. |
| Desktop apps | IMPLEMENTED; BUILD-VERIFIED on current baseline | Six apps built; panel survived the usercopy probe regression | The complete files/editor/FAT workflow is not verified after the recovery change. |
| virtio block | IMPLEMENTED; QEMU-VERIFIED on storage smoke path | FAT storage smoke target and visible-target wiring | Visible desktop attachment is implemented but unverified end-to-end. |
| virtio GPU | IMPLEMENTED; earlier MANUAL-VERIFIED; deterministic runner IMPLEMENTED | `tools/qemu_marker_test.sh fb` asserts GPU/window and panel-ready markers | No real run of the new marker gate is recorded. |
| virtio input | IMPLEMENTED; HOST-VERIFIED | Parser/driver tests | Visible input behavior still requires manual workflow verification. |
| USB xHCI/HID | IMPLEMENTED; HOST-VERIFIED; deterministic runner IMPLEMENTED | USB marker runner requires controller, enumeration, and two HID devices | No real run of the new marker gate; no hub support claim. |
| virtio network/DHCP | IMPLEMENTED; HOST-VERIFIED; deterministic runner IMPLEMENTED | Network runner requires initialization and DHCP ACK | Real end-to-end QEMU DHCP remains `UNVERIFIED`; no sockets, TCP, or HTTP. |
| KLI1 application images | IMPLEMENTED; HOST-VERIFIED | Image layout and shipping blob tests | Mutable `.data`/`.bss` is now explicitly forbidden by the linker script and exercised by `tests/run_kli1_contract_test.sh`. |
| Raspberry Pi 4 board layer | IMPLEMENTED; BUILD-VERIFIED for contract; UNVERIFIED on hardware | virtio-input stubs added; `make BOARD=rpi4` and the `board-rpi4` gate in `tools/verify.sh` both clean | eMMC driver and physical serial milestone still pending (RISK-007). |

## Confirmed implementation facts

The current QEMU codebase includes:

- AArch64 EL1 kernel entry with DTB handoff;
- physical and virtual memory managers;
- EL0 processes with private image, stack, anonymous mappings, and page-table roots;
- preemptive EL0 dispatch on IRQ and voluntary yield;
- cooperative EL1 helper threads; **`docs/ARCHITECTURE.md`, `docs/CODEX_HANDOFF.md`, and `docs/CURRENT_STATE.md` repeat the preemptive-EL0 / cooperative-EL1 contract**, and `kernel/sched/sched.c` exposes the explicit `sched_yield` boundary;
- KLI1 freestanding application images;
- a kernel-owned GUI compositor;
- panel, shell, editor, files, monitor, and clock applications;
- bootfs, tmpfs, and a small FAT32 root filesystem bridge;
- virtio block, GPU, input, and network paths for QEMU;
- PCI/xHCI and boot-protocol HID parsing;
- permission-aware user-copy helpers in `kernel/syscall_helpers.c` (PTE-checked `user_buf_range`, `sys_copy_from_user`, `sys_copy_to_user`, `sys_user_copy_cstr`);
- per-process VFS descriptors in `kernel/vfs.c`, reclaimed centrally through `process_mark_exited`;
- W^X kernel page-table construction in `kernel/mm/vmm.c` via `vmm_map_kernel_identity()`, wired into the bootstrap PGD (kernel.c) and per-process PGDs (panel_boot.c);
- a broad native host test suite;
- a visible QEMU target wired to the generated FAT32 virtio block image;
- a common userland window-create wrapper that requests focus after successful creation;
- a KLI1 mutable-storage contract enforced by `programs/apps/image.ld` ASSERTs and verified by `tests/run_kli1_contract_test.sh`;
- `tools/verify.sh` as the one-command local baseline (now also running `tests/run_vfs_process_fd_test.sh`, `tests/run_user_copy_permissions_test.sh`, `tests/run_kli1_contract_test.sh`, `tests/run_board_build_test.sh`, `tools/qemu_usercopy_test.sh`, `tools/qemu_marker_test.sh all`, and `tools/qemu_fb_fat_test.sh`);
- deterministic serial-marker tools for framebuffer, USB, and DHCP QEMU paths.

These implementation facts do not override the limitations in the subsystem table or active risk register.

## v1.0 blockers

Both syscall-boundary P0 risks, the deterministic QEMU gate scaffold, the visible-desktop FAT wiring, and the focus syscall path are now closed. The remaining v1.0 work is human-driven verification and reproducibility:

1. **RISK-003:** the visible create/edit/save/rename/reopen/delete FAT workflow must be recorded by a named human tester on `make qemu-fb-visible` against a current commit. The deterministic wiring gate (`tools/qemu_fb_fat_test.sh`) already proves the FAT + GPU + panel hot path boots together.
2. **RISK-004:** the visible files-to-editor focus behaviour must be confirmed by a named human tester on `make qemu-fb-visible` against a current commit. The `tools/qemu_focus_test.sh` gate already proves the focus syscall path runs end-to-end for every non-panel app.
3. Complete the visible create/edit/save/rename/reopen/delete FAT workflow (same as RISK-003).
4. Resolve the GitHub Actions pre-step infrastructure failure tracked in issue #12.
5. Reconcile release documentation after the verified blockers are closed.

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
