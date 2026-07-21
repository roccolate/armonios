#!/usr/bin/env python3

from pathlib import Path


def replace_once(old: str, new: str) -> None:
    path = Path("docs/CURRENT_STATE.md")
    text = path.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected one match, found {count}: {old!r}")
    path.write_text(text.replace(old, new, 1))


replace_once(
    '- **Audit date:** 2026-07-19',
    '- **Audit date:** 2026-07-21',
)
replace_once(
    '- **Code baseline synchronized:** v0.1 source tree; current public first commit is `78b9d45` (`v0.1`)',
    '- **Code baseline synchronized:** `main` through `bb37f19`, plus PR #39 code head `fe5ce3bf1fbe4759956896fe513716a5ce4dbe8e`',
)
replace_once(
    '- **Audit method:** repository-wide static inspection plus a local verification run of `bash tools/verify.sh` at `2026-07-19T01:05:06Z` on the equivalent pre-reset source tree, including `board-rpi4`, `process-fd-isolation`, `usercopy-host`, `kli1-contract`, `usercopy-qemu`, `qemu-focus`, `qemu-markers`, and `qemu-fb-fat` automated gates. Serial logs for the QEMU gates live under `build/qemu-*-test.log`, `build-focus/qemu-focus-test.log`, and `build-usercopy-test/qemu-usercopy-test.log`.',
    '- **Audit method:** repository-wide static inspection plus hosted verification of PR #39 code head `fe5ce3b`. `Verify ArmoniOS` run `29811549116` and `CI - Tests` run `29811549104` both completed successfully, covering build/size, the minimal RPi4 EMMC2 probe, native host tests, syscall-boundary regressions, stack checks, FAT32 QEMU smoke, and the complete `tools/verify.sh` matrix.',
)
replace_once(
    '| `make size` | BUILD-VERIFIED | `kernel.bin: 106524 bytes (limit: 108000)`. |',
    '| `make size` | BUILD-VERIFIED | PR #39 code head passes the unchanged 108000-byte kernel limit. |',
)
replace_once(
    '| `make -C tests test` | HOST-VERIFIED | `ALL TESTS PASSED (0)`. |',
    '| `make -C tests test` | HOST-VERIFIED | Native suite passes, including mapped EL0 regressions for VFS, IPC/argv, GUI outputs, information outputs, read-only destinations, and event preservation. |',
)
replace_once(
    '| `.github/workflows/tests.yml` | CI-VERIFIED | Hosted workflow reached runner bootstrap and checkout, installed the cross-toolchain plus `qemu-system-arm`, ran `bash tools/verify.sh`, and uploaded QEMU serial logs. |',
    '| `.github/workflows/tests.yml` | CI-VERIFIED | Run `29811549104` completed `bash tools/verify.sh` and uploaded QEMU serial logs for code head `fe5ce3b`. |',
)
replace_once(
    '| `make BOARD=rpi4` | BUILD-VERIFIED | `tests/run_board_build_test.sh` passes; `build-rpi4/kernel.bin` is 102428 bytes under the 108000-byte limit. |',
    '| `make BOARD=rpi4` and `make rpi4-emmc2-probe` | BUILD-VERIFIED | Normal RPi4 remains fail-closed; the read-only diagnostic probe, telemetry, primary-MBR FAT32 discovery, and bounded block-view tests pass. The previously inspected minimal `kernel8.img` was 73740 bytes under the 108000-byte limit. |',
)
replace_once(
    '| EL0 processes | IMPLEMENTED; HOST-VERIFIED | Process table, saved trap frames, per-process page tables, spawn/wait/kill/exit tests | User-output copies are now PTE-checked via `sys_user_buf_out`; richer stress coverage is still pending. |',
    '| EL0 processes | IMPLEMENTED; HOST-VERIFIED | Process table, saved trap frames, per-process page tables, spawn/wait/kill/exit tests, packed argv import | Permission-aware validation and kernel-owned syscall payloads are implemented; fault-recoverable copyin/copyout is still pending. |',
)
replace_once(
    '| Syscall ABI | IMPLEMENTED; HOST-VERIFIED | Frozen numbers and ABI tests | Output copies enforce per-page write permission via PTE checks; memory hardening implemented under RISK-008. |',
    '| Syscall ABI | IMPLEMENTED; HOST-VERIFIED | Frozen numbers, ABI tests, and mapped EL0 boundary regressions | VFS, argv, IPC, GUI, and information payloads cross through kernel-owned temporaries; final copies remain ordinary non-fault-contained EL1 loads/stores. |',
)
replace_once(
    '| VFS | IMPLEMENTED; HOST-VERIFIED | Per-process descriptors, bootfs/tmpfs/FAT dispatch, and `process-fd-isolation` gate | Fixed-table facade only: no generic mount table, common path resolver, structured directory ABI, or filesystem driver boundary yet. |',
    '| VFS | IMPLEMENTED; HOST-VERIFIED | Per-process descriptors, static nodes, small generic mount table, filesystem callbacks, and `process-fd-isolation` gate | Still fixed-capacity and non-POSIX; no common path resolver or structured directory/metadata ABI. |',
)
replace_once(
    '| FAT32 | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on storage smoke path; MANUAL-VERIFIED on visible workflow | Root 8.3 create/read/write/rename/delete/list plus QEMU mount markers and the existing 2026-07-17 visible workflow | No subdirectories, long names, partition discovery, broad FAT compatibility, or newer manual desktop evidence for the 2026-07-19 automated baseline. |',
    '| FAT32 | IMPLEMENTED; HOST-VERIFIED; QEMU-VERIFIED on storage smoke path; MANUAL-VERIFIED on visible workflow | Root 8.3 create/read/write/rename/delete/list, generic VFS mount dispatch, primary-MBR FAT32 discovery and bounded block-view tests, QEMU mount markers, and the existing 2026-07-17 visible workflow | No subdirectories, long names, GPT/extended partitions, broad FAT compatibility, or physical RPi media evidence. |',
)
replace_once(
    '| Raspberry Pi 4 board layer | IMPLEMENTED; BUILD-VERIFIED for contract; UNVERIFIED on hardware | virtio-input stubs added; `make BOARD=rpi4` and the `board-rpi4` gate in `tools/verify.sh` both clean | eMMC driver and physical serial milestone still pending (RISK-007). |',
    '| Raspberry Pi 4 board layer | IMPLEMENTED; HOST/BUILD-VERIFIED; UNVERIFIED on hardware | SDHCI core, firmware clock query, broken-CD adapter, telemetry, minimal read-only probe, MBR discovery, block view, and board build gates | Normal capabilities remain zero; no physical clock/card/FAT read claim and no writes (RISK-007). |',
)
replace_once(
    '- permission-aware user-copy helpers in `kernel/syscall_helpers.c` (PTE-checked `user_buf_range`, `sys_copy_from_user`, `sys_copy_to_user`, `sys_user_copy_cstr`);',
    '- permission-aware user-copy helpers in `kernel/syscall_helpers.c`, packed argv import, kernel-owned IPC/VFS/GUI/info buffers, and state-preserving output validation;',
)
replace_once(
    '- a broad native host test suite;',
    '- a broad native host test suite, including mapped EL0 boundary tests for VFS, IPC/argv, GUI, and system-information outputs;',
)
replace_once(
    '- Raspberry Pi 4 or Raspberry Pi 5 support;\n- a valid Raspberry Pi SD/eMMC storage driver;',
    '- verified physical Raspberry Pi 4 or Raspberry Pi 5 boot/storage support;\n- writable Raspberry Pi SD/eMMC storage;',
)
replace_once(
    'It currently runs build, size, BOARD=rpi4 build-contract, host tests, process-local VFS FD isolation, the standalone user-copy permissions gate, the KLI1 mutable-storage contract gate, stack checking, the FAT32 storage smoke test, usercopy/focus QEMU gates, framebuffer/USB/network marker gates, and the visible-desktop FAT+GPU wiring gate. A clean run at `2026-07-19T01:05:06Z` was recorded on the equivalent v0.1 pre-reset source tree. QEMU serial logs are under `build/`, `build-focus/`, and `build-usercopy-test/`.',
    'It currently runs build and size checks, normal and probe RPi4 builds, EMMC2 telemetry/MBR/block-view regressions, native host tests, process-local VFS FD isolation, mapped EL0 user-copy boundary tests, the KLI1 mutable-storage contract, stack checking, FAT32 storage smoke, usercopy/focus QEMU gates, framebuffer/USB/network marker gates, and the visible-desktop FAT+GPU wiring gate. Hosted run `29811549104` completed this matrix for code head `fe5ce3b`; run `29811549116` independently passed the shorter build/host/FAT gate.',
)

Path(__file__).unlink()
