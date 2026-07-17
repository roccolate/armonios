# Technical Risk Register

This is the active risk register for ArmoniOS. It records verified defects, static contradictions, and missing evidence that can affect correctness or release claims.

Status and evidence terminology follows `DOCUMENTATION_POLICY.md`.

## Severity

- **P0** — can compromise kernel correctness, isolation, or the core release contract; blocks the affected release.
- **P1** — materially affects stability, reproducibility, or maintainability; must be fixed or explicitly accepted before release.
- **P2** — limited or future-facing issue; track and schedule deliberately.

## Open risks

| ID | Severity | Area | Status | Summary |
|---|---:|---|---|---|
| RISK-003 | P1 | QEMU desktop target | FIX IMPLEMENTED; OPEN PENDING VERIFICATION | `qemu-fb-visible` attaches FAT32, but the visible workflow has not been rerun. |
| RISK-004 | P1 | Desktop focus | FIX IMPLEMENTED; OPEN PENDING VERIFICATION | New userland windows request focus, but files-to-editor has not been rerun. |
| RISK-005 | P1 | Runtime verification | FIX PARTIALLY IN MAIN; OPEN PENDING REAL QEMU | Deterministic framebuffer, USB, and DHCP runners are in `main`; `usercopy-qemu` gate wired into `tools/verify.sh`, per-subsystem marker logs still need real captures. |
| RISK-006 | P1 | Raspberry Pi 4 build | OPEN | The RPi4 board backend does not satisfy the complete board interface. |
| RISK-007 | P0 for hardware track | Raspberry Pi storage | OPEN | The current eMMC implementation is contradictory and not hardware-validated. |
| RISK-008 | P1 | Memory protection | OPEN | Process page tables identity-map the full RAM range as kernel RWX. |
| RISK-009 | P1 | KLI1 application format | OPEN | Mutable `.data` and `.bss` behavior is not an explicit, tested image-format contract. |
| RISK-010 | P2 | Scheduling documentation | OPEN | EL0 processes are preemptive, but EL1 kernel threads are cooperative. |
| RISK-011 | P1 | Verification infrastructure | OPEN | GitHub Actions jobs fail before checkout and expose no step logs. |

## RISK-003 — Visible desktop FAT wiring requires verification

**Severity:** P1
**Affected release:** v1.0 QEMU desktop
**Evidence:** earlier Makefile inspection and implementation commit `662f3ee4032ef09dac6872e1a06e8c60c3ac7611`

The documented visible FAT workflow used `make qemu-fb-visible`, but that target attached GPU and input devices without the generated virtio block image. The `files` application therefore reported FAT as unavailable.

The target now depends on `$(VIRTIO_BLK_IMG)` and attaches it through `virtio-blk-device`. This removes the static wiring defect, but no post-change build or visible QEMU run has been recorded.

**Exit criteria:** `bash tools/verify.sh` passes on the implementation commit and the full create/edit/save/rename/reopen/delete workflow passes in `make qemu-fb-visible`.

## RISK-004 — Spawned editor lacks initial focus

**Severity:** P1
**Affected release:** v1.0 QEMU desktop
**Evidence:** application and GUI code inspection plus issue #1 manual observation; implementation commit `662f3ee4032ef09dac6872e1a06e8c60c3ac7611`

The earlier editor created and titled a window without explicitly focusing it. Window creation only selected a window automatically when no other application window was focused, so keyboard input remained with `files` after spawning the editor.

The common `libkarmdesk` window-create wrapper now requests focus after a successful create. Kernel `GUI_WINDOW_NO_FOCUS` policy remains authoritative because the focus syscall can reject no-focus windows and the wrapper ignores that presentation-only failure.

**Exit criteria:** `bash tools/verify.sh` passes and a named tester confirms that a file opened from `files` accepts keyboard input in `editor` immediately without an extra click.

## RISK-005 — Deterministic QEMU gates need real execution

**Severity:** P1
**Affected release:** v1.0 QEMU desktop
**Evidence:** Makefile inspection; implementation commit `bb7c2eb910dcfecc87990cea7ad3afcdb08ada8b`; `tools/qemu_usercopy_test.sh` implementation commit

The legacy `qemu-fb`, `qemu-usb`, and `qemu-net` Make targets remain launch commands. `main` now also contains evidence-producing runners:

```sh
bash tools/qemu_marker_test.sh fb
bash tools/qemu_marker_test.sh usb
bash tools/qemu_marker_test.sh net
bash tools/verify_qemu.sh
bash tools/qemu_usercopy_test.sh
```

`qemu_usercopy_test.sh` is now wired into `tools/verify.sh` as a mandatory automated gate and validates the kernel-to-user permission boundary end-to-end. The other runners still need real captures for the per-subsystem markers.

**Exit criteria:** `bash tools/verify_qemu.sh` and `bash tools/verify.sh` pass on real QEMU for an exact `main` commit; the per-marker logs are attached to issue #1; `qemu_usercopy_test.sh` is recorded for the same commit.

## RISK-006 — Raspberry Pi board contract is incomplete

**Severity:** P1
**Affected release:** v1.5 hardware bring-up
**Evidence:** static board-interface inspection

The generic board contract requires functions the current RPi4 backend does not provide. The repository must not claim `BOARD=rpi4` is build-verified until a clean compile and link is recorded.

**Exit criteria:** CI or equivalent evidence builds and links `make BOARD=rpi4`; unsupported capabilities return explicit safe failures; no generic kernel symbol remains unresolved.

## RISK-007 — Raspberry Pi eMMC driver is not a valid storage reference

**Severity:** P0 for the hardware track
**Affected release:** v1.5 hardware bring-up
**Evidence:** static driver inspection

The current implementation mixes register offsets and block-size constants, uses inconsistent buffer indexing, constructs command indexes through conflicting constants, and marks the controller ready without a complete SD-card initialization sequence.

This file must be treated as experimental scaffolding, not a working driver.

**Exit criteria:** rewrite or validate the controller sequence against BCM2711 documentation; add pure command/register tests where possible; confirm sector reads on physical hardware before enabling FAT writes.

## RISK-008 — Full RAM is kernel RWX in each process table

**Severity:** P1
**Affected release:** post-v1.0 hardening unless promoted
**Evidence:** static VMM and loader inspection

The bootstrap table and every process page table identity-map detected RAM read/write/execute for EL1. EL0 access is restricted, but kernel W^X is absent and every process duplicates the kernel mapping in TTBR0.

This increases page-table cost, forces global TLB invalidation during switches, and leaves kernel data executable.

**Target direction:** shared kernel mappings through TTBR1, section-specific permissions, ASIDs, and scoped TLB invalidation.

**Exit criteria:** architecture tests and QEMU verification show RX text, R rodata, RW/NX data/heap/stacks, device/NX MMIO, and isolated TTBR0 user mappings.

## RISK-009 — KLI1 mutable storage contract is undefined

**Severity:** P1
**Affected release:** v1.0 or v1.1, depending on whether v1.0 formally forbids mutable static storage
**Evidence:** application linker-script inspection

The KLI1 image script explicitly places header, text, rodata, and end marker. It does not define a tested load contract for initialized `.data` or zero-initialized `.bss`.

Applications currently avoid this through restricted coding patterns and anonymous mappings, but the toolchain does not clearly reject unsupported mutable static storage.

**Exit criteria:** either:

- formally forbid `.data`/`.bss` and make the link fail when they are emitted; or
- extend KLI1 with file-size/memory-size or explicit data/BSS metadata and test loader behavior.

## RISK-010 — Kernel-thread scheduling is cooperative

**Severity:** P2
**Affected release:** documentation accuracy now; implementation only when needed
**Evidence:** scheduler inspection

Timer IRQ preemption applies to EL0 process trap frames. EL1 helper threads change only through explicit yield/exit paths. Documentation must describe the system as preemptive for EL0 and cooperative for kernel threads.

**Exit criteria:** documentation remains accurate, or a separately designed EL1 preemption mechanism is implemented and tested.

## RISK-011 — GitHub Actions does not reach checkout

**Severity:** P1
**Affected release:** v1.0 reproducibility
**Evidence:** repeated pull-request workflow runs for both the historical and replacement workflows

GitHub creates the jobs, but they finish with failure before exposing any step or downloadable job log. The historical `CI - Tests` workflow and the new CI-only PR #11 show the same pattern, so no build or test failure can be inferred from those runs.

Issue #12 tracks the required repository/account-level investigation. Until a runner reaches checkout, local `bash tools/verify.sh` output is the only available automated release evidence; `tools/verify.sh` now also runs the `usercopy-host`, `usercopy-qemu`, and `process-fd-isolation` gates so that the local baseline covers both syscall-boundary P0s.

**Exit criteria:** a GitHub-hosted runner reaches checkout, logs are available, the local verification baseline runs in CI, and the FAT32 serial log is preserved as an artifact.

## Closed risks

No risk may be moved here without the evidence and exit criteria required by `DOCUMENTATION_POLICY.md`.

### RISK-001 (CLOSED 2026-07-16)

**Severity when open:** P0
**Affected release:** v1.0 QEMU desktop
**Closing commit:** `4494c554df212401ffbd294d1a44b5977696fa0b`

**Summary of what was open:** `sys_user_buf_in()` and `sys_user_buf_out()` originally used the same region-membership check. Application image regions are mapped read/execute and stacks read/write, so an output syscall could accept an address inside a registered but non-writable image mapping, and a subsequent EL1 write would fault inside the kernel.

**Closing evidence:**

- `kernel/syscall_helpers.c` (`user_buf_range()`) iterates the process's real EL0 L3 page table, returns `ERR_INVAL` for missing PTEs, and returns `ERR_PERM` when `AP_RO` is set on any covered page of an output request. The validation runs before any byte is written, so a mixed RW→RO range is rejected atomically without partial writes.
- Separate helpers in `kernel/syscall_helpers.h`: `sys_user_buf_in`, `sys_user_buf_out`, `sys_copy_from_user`, `sys_copy_to_user`, `sys_user_copy_cstr`. Every output-producing syscall in `kernel/syscall.c` now reaches user memory through one of these helpers; a manual call-site audit confirmed full coverage.
- Host coverage:
  - `tests/test_syscall_helpers.c` (Unity, runs inside `make -C tests test`) — `test_syscall_helpers_user_buffers_validate_registered_ranges`, `_copy_cstr_validates_each_byte`, `_copy_cstr_rejects_unregistered_tail`, `_owner_window_error_modes_are_stable`.
  - `tests/test_user_copy_permissions_standalone.c` — writable copy succeeds, read-only destination yields `ERR_PERM`, mixed RW→RO range is atomic (no partial writes), missing/non-user PTEs yield `ERR_INVAL`, zero-length range is accepted.
- QEMU coverage: `tools/qemu_usercopy_test.sh` is now part of `tools/verify.sh`. On commit `4494c55` it captured `build-usercopy-test/qemu-usercopy-test.log` containing six `USERCOPY: RX output rejected` probes across distinct EL0 processes, followed by `panel: ready` and `clock: starting`, proving the kernel rejected the invalid output buffer without halting and a second process kept scheduling.

### RISK-002 (CLOSED 2026-07-16)

**Severity when open:** P0
**Affected release:** v1.0 QEMU desktop
**Closing commit:** `4494c554df212401ffbd294d1a44b5977696fa0b`
**Implementation commits on `main`:** `0b6c728 vfs: define process-local descriptor contract`, `ac17418 test: add process-local descriptor regression`, `e97e618 docs: explain process-local descriptor design` (merge `ed8cd07`).

**Summary of what was open:** `kernel/vfs.c` owned one fixed global open-file table with eight slots for the entire kernel. Syscall-visible descriptors did not encode or validate an owner PID, so process A could use, seek, or close process B's descriptor. There was no per-process cleanup on exit/fault/kill.

**Closing evidence:**

- Per-process `owner_pid` recorded on every `vfs_open_file_t` slot, populated by `vfs_open_flags()` from `vfs_current_owner_pid()` at allocation time.
- All four fd APIs route through `vfs_fd_at(fd)`, which compares the slot's `owner_pid` against the current process. A foreign or unknown local descriptor returns `0`, so `vfs_read_fd`, `vfs_write_fd`, `vfs_seek`, and `vfs_close` reject cross-process operations with `-1`. `vfs_reap_dead_owners()` runs lazily inside `vfs_fd_at`, so dead-owner capacity is also reclaimed.
- `vfs_open` returns a local descriptor number scoped to the caller, so two processes can independently hold local fd `0` against different files. Exhaustion is per-process and recycles after the process exits.
- `vfs_close_all_for_pid(uint32_t pid)` walks the global table and clears every slot owned by the pid. It is invoked exclusively from `process_mark_exited()`, which is the central cleanup called by `sys_exit`, `handle_user_fault`, and `process_kill`. A second `process_mark_exited` is a clean no-op (state-gated) so cleanup is idempotent.
- Host coverage:
  - `tests/test_vfs_process_fd_standalone.c` — same local fd number is private to each pid, foreign unused fd is rejected, dead-owner capacity is reclaimed across the VFS_MAX_OPEN_FILES limit.
  - `tests/test_process_fd_cleanup_standalone.c` — `process_mark_exited` invokes `vfs_close_all_for_pid(process->pid)` and the GUI window destroyer exactly once per transition to zombie, and a second invocation is a no-op.
  - Both run as part of `tools/verify.sh` (`process-fd-isolation` gate).
- The `process-fd-isolation` gate is wired into the automated baseline alongside the existing `host-tests`, `usercopy-host`, `usercopy-qemu`, `stack-check`, and `qemu-fs-test` gates, so the regression cannot silently regress on a future commit.
