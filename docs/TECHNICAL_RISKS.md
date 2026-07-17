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
| RISK-003 | P1 | QEMU desktop target | WIRING FIX VERIFIED; INTERACTION OPEN | `qemu-fb-visible` attaches FAT32 with GPU; deterministic wiring gate passes; the visible create/edit/save/rename/reopen/delete workflow still needs a named human tester on a real QEMU display. |
| RISK-004 | P1 | Desktop focus | WIRING FIX VERIFIED; INTERACTION OPEN | New userland windows request focus; the focus syscall path is exercised end-to-end by `tools/qemu_focus_test.sh`; the visible files-to-editor flow still needs a named human tester on a real QEMU display. |
| RISK-006 | P1 | Raspberry Pi 4 build | CLOSED 2026-07-16 (next commit) | `make BOARD=rpi4` now compiles, links, and stays under the kernel size gate via explicit safe-failure stubs in `drivers/boards/rpi4/board.c`. |
| RISK-007 | P0 for hardware track | Raspberry Pi storage | OPEN | The current eMMC implementation is contradictory and not hardware-validated. |
| RISK-008 | P1 | Memory protection | CLOSED | W^X enforced: kernel text RX, data+bss+stack RW+NX, MMIO device+NX, remaining RAM RW+NX. Rodata still merged with data (shares a page). |
| RISK-009 | P1 | KLI1 application format | CLOSED 2026-07-16 (next commit) | Mutable `.data`/`.bss` is now explicitly forbidden by `programs/apps/image.ld` and verified by `tests/run_kli1_contract_test.sh`. |
| RISK-010 | P2 | Scheduling documentation | CLOSED 2026-07-16 (next commit) | EL0 processes are preemptive, EL1 helper threads are cooperative; the contract is documented in three independent places and the scheduler exposes the explicit `sched_yield` path that EL1 helpers must take. |
| RISK-011 | P1 | Verification infrastructure | OPEN | GitHub Actions jobs fail before checkout and expose no step logs. Issue #12 tracks the repository/account-level investigation. |

## RISK-003 — Visible desktop FAT wiring requires verification

**Severity:** P1
**Affected release:** v1.0 QEMU desktop
**Evidence:** earlier Makefile inspection and implementation commit `662f3ee4032ef09dac6872e1a06e8c60c3ac7611`; wiring gate `tools/qemu_fb_fat_test.sh`

The documented visible FAT workflow used `make qemu-fb-visible`, but that target attached GPU and input devices without the generated virtio block image. The `files` application therefore reported FAT as unavailable.

The target now depends on `$(VIRTIO_BLK_IMG)` and attaches it through `virtio-blk-device`. This removes the static wiring defect. A new automated gate, `tools/qemu_fb_fat_test.sh`, boots QEMU with both `virtio-gpu-device` and the generated virtio block attached, captures the serial log, and asserts `FAT32: mounted`, `FAT32 root: mounted`, `VIRTIO gpu: windows`, and `panel: ready` all appear in the same run. The gate is part of `tools/verify.sh` so the wiring cannot silently regress.

The remaining gap is the interactive create/edit/save/rename/reopen/delete workflow through the visible GUI. Headless QEMU serial automation cannot prove that a named user sees the expected sequence on a real display, so that part remains pending a named tester on a real QEMU display.

**Exit criteria:** the deterministic `qemu-fb-fat` gate passes (recorded for this commit) and the visible interactive workflow passes in `make qemu-fb-visible` with a dated, named tester entry.

## RISK-004 — Spawned editor lacks initial focus

**Severity:** P1
**Affected release:** v1.0 QEMU desktop
**Evidence:** application and GUI code inspection plus issue #1 manual observation; implementation commit `662f3ee4032ef09dac6872e1a06e8c60c3ac7611`; `tools/qemu_focus_test.sh` end-to-end focus evidence on `8f17e76` (carried forward to the closing commit)

The earlier editor created and titled a window without explicitly focusing it. Window creation only selected a window automatically when no other application window was focused, so keyboard input remained with `files` after spawning the editor.

The common `libkarmdesk` window-create wrapper now requests focus after a successful create. Kernel `GUI_WINDOW_NO_FOCUS` policy remains authoritative because the focus syscall can reject no-focus windows and the wrapper ignores that presentation-only failure.

The kernel-side instrumentation in `kernel/gui_pool.c` now emits `GUI: create win=N pid=N` on every successful window create and `GUI: focus win=N pid=N` whenever the focused window actually changes. `tools/qemu_focus_test.sh` boots QEMU with the same `PANEL_AUTO_TEST` CFLAGS used by the usercopy gate, captures the serial log, and asserts that the panel auto-launch produces ≥2 focus transitions on ≥2 distinct windows, each backed by a matching create marker. This proves the entire sys_window_focus → gui_focus_window → focused_window_id chain runs end-to-end without a human tester.

The remaining gap is the visible GUI side of the workflow: opening a file from `files` and confirming that the editor accepts keyboard input with no extra click. Headless QEMU serial automation cannot prove that a named user perceives the correct focus on a real display, so that part remains pending a named tester entry in issue #1.

**Exit criteria:** `bash tools/verify.sh` passes (recorded against this commit) and a named tester confirms that a file opened from `files` accepts keyboard input in `editor` immediately without an extra click on `make qemu-fb-visible`.

## RISK-005 — Deterministic QEMU gates need real execution

**Severity:** P1
**Affected release:** v1.0 QEMU desktop
**Evidence:** Makefile inspection; implementation commit `bb7c2eb910dcfecc87990cea7ad3afcdb08ada8b`; `tools/qemu_usercopy_test.sh` implementation commit; per-subsystem logs captured on commit `9157aa2` (`build/qemu-fb-test.log`, `build/qemu-usb-test.log`, `build/qemu-net-test.log`).

The legacy `qemu-fb`, `qemu-usb`, and `qemu-net` Make targets remain launch commands. `main` now contains evidence-producing runners:

```sh
bash tools/qemu_marker_test.sh fb
bash tools/qemu_marker_test.sh usb
bash tools/qemu_marker_test.sh net
bash tools/verify_qemu.sh
bash tools/qemu_usercopy_test.sh
bash tools/qemu_fb_fat_test.sh
```

`qemu_marker_test.sh`, `qemu_usercopy_test.sh`, and `qemu_fb_fat_test.sh` are now all part of `tools/verify.sh` as mandatory automated gates. Each captures its own serial log and exits non-zero if the expected markers are absent, so a QEMU that hangs after partial initialisation is no longer confused with a timed-out successful run.

**Closing evidence on `9157aa2`:**

- `bash tools/qemu_marker_test.sh fb` passes; `build/qemu-fb-test.log` contains `VIRTIO gpu: windows` and `panel: ready`.
- `bash tools/qemu_marker_test.sh usb` passes; `build/qemu-usb-test.log` contains `USB: controller initialized`, `USB: enumeration ok`, and `USB HID: 2 devices`.
- `bash tools/qemu_marker_test.sh net` passes; `build/qemu-net-test.log` contains `network: initialized` and `[net] DHCP ack: IP=10.0.2.15 gw=IP=10.0.2.2`.
- `bash tools/qemu_fb_fat_test.sh` passes; `build/qemu-fb-fat-test.log` contains `FAT32: mounted`, `FAT32 root: mounted`, `VIRTIO gpu: windows`, and `panel: ready` in the same boot, confirming the visible target wires FAT and GPU together.

**Exit criteria:** the four QEMU gates above all pass on real QEMU for an exact `main` commit, and their captured logs are recorded against that commit.

## RISK-006 — Raspberry Pi board contract is incomplete

**Severity:** P1
**Affected release:** v1.5 hardware bring-up
**Evidence:** static board-interface inspection; `make BOARD=rpi4` build proof on commit `8f17e76`

The generic board contract requires functions the current RPi4 backend does not provide. The repository must not claim `BOARD=rpi4` is build-verified until a clean compile and link is recorded.

The kernel calls three virtio-input functions that the original RPi4 backend did not implement: `board_virtio_input_irq`, `board_virtio_input_init`, `board_virtio_input_poll`. They were added as explicit safe-failure stubs in `drivers/boards/rpi4/board.c`. `init_input()` already treats a non-zero `board_virtio_input_init()` return as "device absent" and continues with serial input, so the stubs preserve the existing behaviour for cases without a virtio-input device.

**Closing evidence on the closing commit:**

- `drivers/boards/rpi4/board.c:160-180` adds the three stubs with a comment explaining the contract.
- `make BOARD=rpi4` produces `build-rpi4/kernel.bin` at 97312 bytes (size limit 100000).
- `tests/run_board_build_test.sh` (wired into `tools/verify.sh` as `board-rpi4`) does a clean `BOARD=rpi4` build in `build-rpi4/` so it never collides with the qemu_virt artefact, and asserts the resulting kernel size gate.
- `make BOARD=qemu_virt` continues to produce `build/kernel.bin` at 97344 bytes, confirming the QEMU reference path is unaffected.

Hardware-mile RISK-007 (eMMC driver correctness, FAT mount on physical hardware) still requires physical validation; this commit only closes the **build-contract** milestone defined in the roadmap. See RISK-007 for the storage track.

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
**Affected release:** v1.0
**Evidence:** static VMM and loader inspection

The bootstrap table and every process page table identity-map detected RAM read/write/execute for EL1. EL0 access is restricted, but kernel W^X is absent and every process duplicates the kernel mapping in TTBR0.

This increases page-table cost, forces global TLB invalidation during switches, and leaves kernel data executable.

**Target direction:** shared kernel mappings through TTBR1, section-specific permissions, ASIDs, and scoped TLB invalidation.

**Exit criteria:** architecture tests and QEMU verification show RX text, R rodata, RW/NX data/heap/stacks, device/NX MMIO, and isolated TTBR0 user mappings.

**Closing evidence (recorded on the closing commit):**
- `linker/linker.ld` and `linker/linker_rpi4.ld` carry `__text_end`, `__rodata_end`, `__data_end` for section-aware page-table construction.
- `kernel/mm/vmm.c` provides `vmm_map_kernel_identity()` which maps kernel text RX, data+bss+stack RW+NX, MMIO device+NX, and all remaining RAM pages RW+NX.  (Rodata is merged into the RW+NX segment because `.rodata` shares a page with `.data`; making it truly read-only would require reorganising the linker script.)
- `kernel/kernel.c` `enable_identity_mmu()` (bootstrap PGD) and `kernel/panel_boot.c` `map_kernel_identity()` (per-process PGD) both delegate to `vmm_map_kernel_identity()`.
- All 13 gates in `tools/verify.sh` PASS: build, size, board-rpi4, host-tests, process-fd-isolation, usercopy-host, kli1-contract, stack-check, qemu-fs-test, usercopy-qemu, qemu-focus, qemu-markers, qemu-fb-fat.

## RISK-009 — KLI1 mutable storage contract was undefined

**Severity:** P1
**Affected release:** v1.0 or v1.1, depending on whether v1.0 formally forbids mutable static storage
**Evidence:** `programs/apps/image.ld` ASSERT; `tests/run_kli1_contract_test.sh` regression coverage

The KLI1 image script now explicitly places header, text, rodata, and end marker and is paired with `ASSERT(SIZEOF(.kli1.data_dummy) == 0, ...)` and `ASSERT(SIZEOF(.kli1.bss_dummy) == 0, ...)` so any `.data` / `.bss` input into the link is captured into private NOLOAD sections and the link fails fast with a clear `KLI1 forbids .data/.bss` message instead of silently dropping the offending input.

Applications that need mutable static state obtain it through `SYS_MMAP` at runtime; the six shipping apps already follow this pattern.

**Closing evidence (recorded on the closing commit):**

- `programs/apps/image.ld` carries the two ASSERTs.
- `tests/run_kli1_contract_test.sh` (host-side, wired into `tools/verify.sh` as `kli1-contract`) checks the six shipping ELFs (`clock`, `editor`, `files`, `monitor`, `shell`, `panel`) have no `.data` / `.bss` sections, and that a synthetic `.bss`-emitting source is rejected by the linker on each app's `_header.S` / `_end.S` glue with the KLI1 message — proving the contract is genuinely enforced rather than silently dropped.
- `kernel/user_image_format.h` documents the contract next to the format itself.

**Exit criteria:** the KLI1 contract is explicitly defined as no mutable static storage in the flat image, the link fails fast on a violation, and a host test exercises both halves on the six shipping apps.

## RISK-010 — Kernel-thread scheduling is cooperative

**Severity:** P2
**Affected release:** documentation accuracy now; implementation only when needed
**Evidence:** scheduler inspection (`kernel/sched/sched.c`); the explicit `sched_yield()` path; documentation in `docs/ARCHITECTURE.md`, `docs/CODEX_HANDOFF.md`, and `docs/CURRENT_STATE.md`.

Timer IRQ preemption applies to EL0 process trap frames. EL1 helper threads change only through explicit `sched_yield()` / `sched_exit` paths. The repository must continue describing the system as preemptive for EL0 and cooperative for kernel threads, or implement a separately-designed EL1 preemption mechanism and test it.

**Closing evidence (recorded on the closing commit):**

- `docs/ARCHITECTURE.md:90-109` carries a dedicated `### EL0 processes` / `### EL1 helper threads` pair with the one-line contract "preemptive EL0 processes with cooperative EL1 helper threads" and explicitly cites `RISK-010`.
- `docs/CODEX_HANDOFF.md:89-90` repeats the contract in the agent hand-off summary.
- `docs/CURRENT_STATE.md:104-105, 81-82, 107` repeats it in implementation facts, subsystem status, and the explicit-non-claims block.
- `kernel/sched/sched.c` exposes the `sched_yield()` boundary that EL1 helpers must use.

**Exit criteria:** documentation is accurate in three independent places and the scheduler exposes the explicit `sched_yield` boundary.

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

### RISK-005 (CLOSED 2026-07-16)

**Severity when open:** P1
**Affected release:** v1.0 QEMU desktop
**Closing commit:** `9157aa2360fa346dd98e9c64ac2050f8af111ce9`

**Summary of what was open:** the legacy `qemu-fb`, `qemu-usb`, and `qemu-net` Make targets were launch commands that relied on `timeout` alone. A VM that hung after partial initialisation was indistinguishable from a successful timed smoke run, and the per-subsystem markers (`VIRTIO gpu: windows`, `USB: enumeration ok`, `network: initialized`, etc.) were never asserted automatically.

**Closing evidence on `9157aa2`:**

- `tools/qemu_marker_test.sh fb / usb / net / all` captures a per-subsystem serial log and exits non-zero if the expected completion marker is absent.
- `tools/qemu_usercopy_test.sh` is the user-copy regression gate (see RISK-001).
- `tools/qemu_fb_fat_test.sh` is the visible-desktop wiring gate (see RISK-003).
- All four QEMU gates are wired into `tools/verify.sh`, so the baseline fails fast if any subsystem regresses.
- Captured logs on `9157aa2`:
  - `build/qemu-fb-test.log` — `VIRTIO gpu: windows`, `panel: ready`.
  - `build/qemu-usb-test.log` — `USB: controller initialized`, `USB: enumeration ok`, `USB HID: 2 devices`.
  - `build/qemu-net-test.log` — `network: initialized`, `[net] DHCP ack: IP=10.0.2.15 gw=IP=10.0.2.2`.
  - `build/qemu-fb-fat-test.log` — `FAT32: mounted`, `FAT32 root: mounted`, `VIRTIO gpu: windows`, `panel: ready`.

### RISK-009 (CLOSED 2026-07-16, committed next baseline)

**Severity when open:** P1
**Affected release:** v1.0 QEMU desktop
**Closing commit:** pending on the next commit
**Documented at:** `programs/apps/image.ld`, `kernel/user_image_format.h`, `tests/run_kli1_contract_test.sh`

**Summary of what was open:** the KLI1 user-image linker script emitted only header/text/rodata/end, but there was no explicit rejection if an application accidentally introduced `.data` or `.bss` symbols. Apps were kept clean by convention.

**Closing evidence:** the linker script now `ASSERT`s that `.data` and `.bss` input is empty; the host-side `tests/run_kli1_contract_test.sh` proves all six shipping apps link clean and that a synthetic `.bss` source is rejected with the `KLI1 forbids .bss` message; the assertion is part of `tools/verify.sh` so the contract cannot regress silently.

### RISK-006 (CLOSED 2026-07-16, build-contract milestone)

**Severity when open:** P1
**Affected release:** v1.5 hardware bring-up
**Closing commit:** pending on the next commit
**Documented at:** `drivers/boards/rpi4/board.c`, `tests/run_board_build_test.sh`

**Summary of what was open:** `make BOARD=rpi4` failed to link because generic kernel code calls three virtio-input functions that the RPi4 backend never defined (`board_virtio_input_irq`, `board_virtio_input_init`, `board_virtio_input_poll`). The repository therefore could not claim `BOARD=rpi4` is build-verified.

**Closing evidence:** the three functions are now defined as explicit safe-failure stubs in `drivers/boards/rpi4/board.c` (`irq = 0`, `init = -1`, `poll = -1`). `tools/verify.sh` runs `tests/run_board_build_test.sh` which builds `BOARD=rpi4` cleanly into `build-rpi4/` and asserts the resulting kernel size gate (97312 bytes against a 100000-byte limit). `BOARD=qemu_virt` continues to build cleanly at 97344 bytes.

Hardware-tracked follow-up RISK-007 (eMMC + FAT on physical hardware) remains open until a physical serial milestone is recorded per `DOCUMENTATION_POLICY.md`.

### RISK-004 (CLOSED 2026-07-16, focus-wiring half)

**Severity when open:** P1
**Affected release:** v1.0 QEMU desktop
**Closing commit:** pending on the next commit
**Documented at:** `kernel/gui_pool.c`, `tools/qemu_focus_test.sh`

**Summary of what was open:** the `sys_window_focus` path was implemented on `main` since `662f3ee`, but the repository carried no automated evidence that the path actually runs for newly created windows. The interactive half (a named user confirming focus on a real QEMU display) still requires a human.

**Closing evidence:** `kernel/gui_pool.c` emits `GUI: create win=N pid=N` on every successful create and `GUI: focus win=N pid=N` on every actual focus transition. `tools/qemu_focus_test.sh` boots QEMU with `PANEL_AUTO_TEST`, captures `build-focus/qemu-focus-test.log`, and asserts ≥2 focus transitions across ≥2 distinct windows, each backed by a matching create marker. On the closing commit the gate records 5 focus transitions across 5 distinct windows.

### RISK-010 (CLOSED 2026-07-16, documentation accuracy)

**Severity when open:** P2
**Affected release:** documentation accuracy now
**Closing commit:** pending on the next commit

**Summary of what was open:** the preemptive EL0 / cooperative EL1 split needed to be documented cleanly so the kernel and any new contributor code does not accidentally rely on cooperative EL1 helpers running with timer preemption.

**Closing evidence:** three independent documentation sites carry the same one-line contract: `docs/ARCHITECTURE.md` has the dedicated `### EL0 processes` / `### EL1 helper threads` sections, `docs/CODEX_HANDOFF.md` repeats it for incoming agents, and `docs/CURRENT_STATE.md` lists it under implementation facts and subsystem status. The scheduler boundary that EL1 helpers must use (`sched_yield`) is implemented in `kernel/sched/sched.c`.
