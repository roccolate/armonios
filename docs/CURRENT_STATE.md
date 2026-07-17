# Current State

> Operational source of truth for ArmoniOS.
>
> Documentation status rules: `DOCUMENTATION_POLICY.md`
> Active correctness and release risks: `TECHNICAL_RISKS.md`

## Audit metadata

- **Project state:** v0.9 QEMU desktop alpha
- **Release target:** v1.0 QEMU desktop release candidate
- **Audit date:** 2026-07-17
- **Code baseline synchronized:** working tree based on `8c8400bcddd754d879e6e21b787b8d028a6c6036`
- **Audit method:** repository-wide static inspection plus a fresh local verification run of `bash tools/verify.sh`, including `board-rpi4`, `process-fd-isolation`, `usercopy-host`, `kli1-contract`, `usercopy-qemu`, `qemu-focus`, `qemu-markers`, and `qemu-fb-fat` automated gates. Serial logs for the QEMU gates live under `build/qemu-*-test.log`, `build-focus/qemu-focus-test.log`, and `build-usercopy-test/qemu-usercopy-test.log`.

## Verification record (2026-07-17 working tree)

| Check | Status | Evidence |
|---|---|---|
| `make` | BUILD-VERIFIED | Baseline build passes for `BOARD=qemu_virt`. |
| `make size` | BUILD-VERIFIED | `kernel.bin: 106524 bytes (limit: 108000)`. |
| `make -C tests test` | HOST-VERIFIED | `ALL TESTS PASSED (0)`. |
| `bash tests/run_vfs_process_fd_test.sh` | HOST-VERIFIED | `process-local VFS descriptors` and `process exit closes VFS descriptors`. |
| `bash tests/run_user_copy_permissions_test.sh` | HOST-VERIFIED | writable copies, `ERR_PERM` on read-only destinations, mixed RW→RO atomicity. |
| `bash tests/run_kli1_contract_test.sh` | HOST-VERIFIED | Seven shipping ELFs have no `.data`/`.bss`; synthetic `.bss` regressions are rejected for every app. |
| `make stack-check` | HOST-VERIFIED | Maximum 368 bytes in `editor` with a 3072-byte limit. |
| `make qemu-fs-test` | QEMU-VERIFIED | `storage: initialized`, `FAT32: mounted`, `FAT32 root: mounted`, `FAT32 shell bytes`, `FAT32 edit file: mounted`, `storage app image: FAT32`. |
| `bash tools/qemu_usercopy_test.sh` | QEMU-VERIFIED | Seven `USERCOPY: RX output rejected` probes across distinct EL0 processes followed by `panel: ready` and `clock: starting`. Log: `build-usercopy-test/qemu-usercopy-test.log`. |
| `bash tools/qemu_focus_test.sh` | QEMU-VERIFIED | Six focus transitions across six distinct windows; every focused window has a matching `GUI: create` marker. Log: `build-focus/qemu-focus-test.log`. |
| `bash tools/qemu_marker_test.sh all` | QEMU-VERIFIED | `qemu-fb` (`display: windows`, `panel: ready`), `qemu-usb` (`USB: controller initialized`, `USB: enumeration ok`, `USB HID: 2 devices`), `qemu-net` (`network: initialized`, `[net] DHCP ack: IP=10.0.2.15`). |
| `bash tools/qemu_fb_fat_test.sh` | QEMU-VERIFIED | Visible-desktop wiring: `FAT32: mounted`, `FAT32 root: mounted`, `display: windows`, `panel: ready` in the same boot. |
| `.github/workflows/tests.yml` | CONFIGURED; UNVERIFIED REMOTE | The workflow now emits a runner-bootstrap step before checkout, installs the cross-toolchain plus `qemu-system-arm`, runs `bash tools/verify.sh`, and uploads QEMU serial logs. No successful GitHub-hosted run is recorded in this audit. |
| `make qemu-fb-visible` interactive workflow | MANUAL-VERIFIED | rocco manually verified Files `/fat` listing, 8.3 create, Editor open/focus/type/Ctrl-S, close, rename, reopen with content intact, delete, refresh, and no stale titlebar artifacts on 2026-07-17. Editor appeared to show one visible text line; save/reopen persistence still passed. |
| `make BOARD=rpi4` | BUILD-VERIFIED | `tests/run_board_build_test.sh` passes; `build-rpi4/kernel.bin` is 102428 bytes under the 108000-byte limit. |
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

The latest local verification recorded for the current working tree:

| Check | Status | Evidence |
|---|---|---|
| `make` | BUILD-VERIFIED | Clean `make BOARD=qemu_virt`. |
| `make size` | BUILD-VERIFIED | `kernel.bin: 106524 bytes (limit: 108000)`. |
| `make -C tests test` | HOST-VERIFIED | `ALL TESTS PASSED (0)`; Unity set now includes `test_syscall_helpers_user_buffers_validate_registered_ranges` and the standalone runner covers the mixed RW→RO atomicity path. |
| `bash tests/run_vfs_process_fd_test.sh` | HOST-VERIFIED | `process-local VFS descriptors` and `process exit closes VFS descriptors` both pass. |
| `bash tests/run_user_copy_permissions_test.sh` | HOST-VERIFIED | New standalone gate: writable copies succeed, read-only destinations yield `ERR_PERM`, mixed RW→RO range is rejected atomically, missing PTEs yield `ERR_INVAL`. |
| `bash tests/run_kli1_contract_test.sh` | HOST-VERIFIED | Each of the seven shipping apps has no `.data`/`.bss` sections in the linked ELF, and a synthetic `.bss` source is rejected by the linker `ASSERT` with the KLI1 message. |
| `make stack-check` | HOST-VERIFIED | Maximum reported stack use: 368 bytes in `editor` with a 3072-byte limit. |
| `make qemu-fs-test` | QEMU-VERIFIED | Fat32 storage smoke test still passes; result captured as part of `bash tools/verify.sh`. |
| `bash tools/qemu_usercopy_test.sh` | QEMU-VERIFIED | Captured `build-usercopy-test/qemu-usercopy-test.log` contains seven `USERCOPY: RX output rejected` probes across distinct EL0 processes followed by `panel: ready` and `clock: starting`. |
| `bash tools/qemu_focus_test.sh` | QEMU-VERIFIED | Captured `build-focus/qemu-focus-test.log` records six focus transitions across six distinct windows; every focused window has a matching `GUI: create` marker. |
| `bash tools/qemu_marker_test.sh all` | QEMU-VERIFIED | Framebuffer, USB, and DHCP marker runners pass with logs under `build/qemu-*-test.log`. |
| `make qemu-fb-visible` | MANUAL-VERIFIED | The target attaches the FAT32 image and new userland windows request focus; rocco verified the create/edit/save/rename/reopen/delete workflow and editor focus without an extra click on 2026-07-17. |
| `make BOARD=rpi4` | BUILD-VERIFIED | `tests/run_board_build_test.sh` (the `board-rpi4` gate inside `tools/verify.sh`) does a clean BOARD=rpi4 build into `build-rpi4/` and asserts the 108000-byte kernel size gate. The RPi4 backend exposes explicit safe-failure stubs for virtio-input; eMMC and physical serial are still pending. |
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
| GUI compositor | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on the focus path; MANUAL-VERIFIED on visible workflow | Windows, ownership, focus, drag, backing buffers, damage, events, and a visible Files-to-Editor pass; `tools/qemu_focus_test.sh` proves the focus syscall path runs end-to-end | Broader visual polish remains future work. |
| Desktop apps | IMPLEMENTED; BUILD-VERIFIED; QEMU-VERIFIED on launch/focus markers; MANUAL-VERIFIED on FAT workflow | Seven apps built: panel, shell, editor, files, monitor, control, and clock; panel survived the usercopy probe regression; focus gate covers six user-visible app windows; rocco verified the Files/Editor/FAT workflow | Editor appears to show one visible text line; treat as v1.1 polish unless it blocks a concrete workflow. |
| virtio block | IMPLEMENTED; QEMU-VERIFIED on storage smoke path | FAT storage smoke target and visible-target wiring | Visible desktop attachment is implemented but unverified end-to-end. |
| virtio GPU | IMPLEMENTED; QEMU-VERIFIED on marker gate | `tools/qemu_marker_test.sh fb` asserts GPU/window and panel-ready markers | Visible manual workflow evidence is still separate. |
| virtio input | IMPLEMENTED; HOST-VERIFIED | Parser/driver tests | Visible input behavior still requires manual workflow verification. |
| USB xHCI/HID | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on marker gate | USB marker runner requires controller, enumeration, and two HID devices | No hub support claim. |
| virtio network/DHCP | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on marker gate | Network runner requires initialization and DHCP ACK | No sockets, TCP, DNS, or HTTP application API. |
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
- panel, shell, editor, files, monitor, control, and clock applications;
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

Both syscall-boundary P0 risks, the deterministic QEMU gate scaffold, the visible-desktop FAT workflow, and the focus path are now closed. The remaining v1.0 work is CI-hosted reproducibility:

1. Run the updated GitHub Actions baseline on a hosted runner and record checkout logs plus QEMU serial-log artifacts for RISK-011.
2. Reconcile release documentation after the remaining verified blocker is closed.

## Explicit non-claims

ArmoniOS does not currently claim:

- production security or hardened process isolation;
- general FAT32 compatibility;
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

It currently runs build, size, BOARD=rpi4 build-contract, host tests, process-local VFS FD isolation, the standalone user-copy permissions gate, the KLI1 mutable-storage contract gate, stack checking, the FAT32 storage smoke test, usercopy/focus QEMU gates, framebuffer/USB/network marker gates, and the visible-desktop FAT+GPU wiring gate. A clean run on 2026-07-17 captured commit/date output via the script's built-in header. QEMU serial logs are under `build/`, `build-focus/`, and `build-usercopy-test/`.

The deterministic QEMU subsystem runner is:

```sh
bash tools/verify_qemu.sh
```

It captures separate serial logs and requires explicit completion markers for framebuffer, USB, and DHCP. The 2026-07-17 `tools/verify.sh` run includes those markers.

Manual visible verification uses the target that now includes GPU, input, and FAT storage:

```sh
make qemu-fb-visible
```

The target wiring and interactive create/edit/save/rename/reopen/delete workflow were manually verified by rocco on 2026-07-17 against working-tree baseline `8c8400bcddd754d879e6e21b787b8d028a6c6036`.

## Maintenance rule

Update this document only from evidence:

- record the exact branch or commit;
- record commands actually run;
- name the person who performed manual or hardware checks;
- attach or reference generated serial logs;
- leave unrun checks as `UNVERIFIED`;
- update `TECHNICAL_RISKS.md` in the same change when a blocker changes state.

Do not infer release readiness from a merged test tool, a timeout, a closed historical review, or a code comment.
