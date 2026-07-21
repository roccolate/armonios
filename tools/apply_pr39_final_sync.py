#!/usr/bin/env python3

from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    target = Path(path)
    text = target.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    target.write_text(text.replace(old, new, 1))


replace_once(
    "kernel/syscall_gui.c",
    '#include "kernel/gui.h"\n#include "kernel/syscall_helpers.h"',
    '#include "kernel/gui.h"\n#include "kernel/kstring.h"\n#include "kernel/syscall_helpers.h"',
)
replace_once(
    "kernel/syscall_gui.c",
    '''    status = sys_user_buf_out(process, out_ptr, sizeof(out));
    if (status != 0) {
        return status;
    }
    status = sys_owner_window(process, window_id, &desktop, &window);
''',
    '''    status = sys_owner_window(process, window_id, &desktop, &window);
''',
)
replace_once(
    "kernel/syscall_gui.c",
    '''    status = sys_user_buf_out(process, out_ptr, sizeof(state));
    if (status != 0) {
        return status;
    }
    if (process == 0 || window_id >= GUI_MAX_WINDOWS) {
''',
    '''    if (process == 0 || window_id >= GUI_MAX_WINDOWS) {
''',
)
replace_once(
    "kernel/syscall_gui.c",
    '''    status = sys_user_buf_out(process, buf_ptr,
                              buf_count * 3U * sizeof(uint32_t));
''',
    '''    status = sys_user_buf_out(process, buf_ptr,
                              buf_count * sizeof(gui_event_t));
''',
)
replace_once(
    "kernel/syscall_gui.c",
    '''    while (n < buf_count && window->event_count > 0) {
        gui_event_t ev;
        uint32_t out[3];

        if (gui_window_pop_event(window, &ev) != 0) {
            break;
        }
        out[0] = ev.type;
        out[1] = (uint32_t)ev.data1;
        out[2] = (uint32_t)ev.data2;
        status = sys_copy_to_user(process, buf_ptr + n * sizeof(out),
                                  out, sizeof(out));
        if (status != 0) {
            return status;
        }
        n++;
    }
''',
    '''    while (n < buf_count && window->event_count > 0) {
        gui_event_t ev;

        if (gui_window_pop_event(window, &ev) != 0) {
            break;
        }
        kmemcpy((void *)(uintptr_t)(buf_ptr + n * sizeof(ev)),
                &ev, sizeof(ev));
        n++;
    }
''',
)
replace_once(
    "kernel/syscall_info.c",
    '#include "kernel/mm/pmm.h"\n#include "kernel/sched/sched.h"',
    '#include "kernel/kstring.h"\n#include "kernel/mm/pmm.h"\n#include "kernel/sched/sched.h"',
)
replace_once(
    "kernel/syscall_info.c",
    '''        status = sys_copy_to_user(
            process, entries_ptr + written * sizeof(syscall_proc_entry_t),
            &entry, sizeof(entry));
        if (status != 0) {
            return status;
        }
        written++;
''',
    '''        kmemcpy((void *)(uintptr_t)(
                    entries_ptr + written * sizeof(syscall_proc_entry_t)),
                &entry, sizeof(entry));
        written++;
''',
)

replace_once(
    "docs/SYSCALLS.md",
    '''### User pointer checks are permission-aware, not fault-contained

Every syscall pointer is checked against the current process's registered user ranges and page table. This prevents one process from directly passing an address that belongs only to another process.

The helper layer distinguishes:

- readable source memory;
- writable destination memory;
- executable image memory.

`sys_user_buf_in()` requires valid EL0-readable pages. `sys_user_buf_out()` additionally rejects read-only pages with `ERR_PERM` before writing any byte. The remaining limitation is that actual byte copies are still normal kernel loads/stores, not fault-contained copyin/copyout.
''',
    '''### User pointers cross a kernel-owned boundary

Every syscall pointer is checked against the current process's registered user ranges and page table. This prevents one process from directly passing an address that belongs only to another process.

The helper layer distinguishes:

- readable source memory;
- writable destination memory;
- executable image memory.

`sys_user_buf_in()` requires valid EL0-readable pages. `sys_user_buf_out()` additionally rejects read-only pages with `ERR_PERM` before writing any byte.

Syscall entry points copy input payloads into bounded kernel temporaries before lower layers use them. Output calls first build kernel-owned values. Calls that consume state, such as IPC receive and window-event receive, validate the complete destination before dequeuing data. Most paths use `sys_copy_from_user()` or `sys_copy_to_user()`; already validated state-consuming paths use the shared `kmemcpy()` primitive for the final bounded copy.

The remaining limitation is fault containment: these byte copies are ordinary kernel loads/stores, not exception-recoverable copyin/copyout routines.
''',
)
replace_once(
    "docs/SYSCALLS.md",
    '| 8 | `sys_spawn_argv` | `x0=path, x1=entry_index, x2=argv_ptr, x3=argc` | PID/error | Spawn a KLI1 image and copy a bounded argv payload to the new stack. |',
    '| 8 | `sys_spawn_argv` | `x0=path, x1=entry_index, x2=argv_ptr, x3=argc` | PID/error | Import argv into a pointer-free kernel block, then build the child argv on its new stack. |',
)
replace_once(
    "docs/SYSCALLS.md",
    '- argv is capped at 8 strings and 256 bytes total payload;',
    '- argv is capped at 8 strings and 256 bytes total payload; the loader receives kernel-owned bytes plus offsets, never caller addresses;',
)
replace_once(
    "docs/SYSCALLS.md",
    '- destination output validation is permission-aware but not fault-contained.',
    '- send copies the source payload before queueing; receive validates the destination before removing the message; copies remain non-fault-contained.',
)
replace_once(
    "docs/SYSCALLS.md",
    'Any window syscall that writes results or events to user memory uses writable-page validation before copying. The copy itself is not fault-contained.',
    'Window bounds and state are assembled in kernel temporaries before copy-out. `sys_window_event` validates the complete destination before removing the first event, so a read-only or invalid output does not consume input. The final copy is not fault-contained.',
)
replace_once(
    "docs/SYSCALLS.md",
    'These calls validate writable user pages before copying their output. The copy itself is not fault-contained.',
    'These calls assemble results in kernel temporaries. `sys_proclist` validates the complete requested array before enumeration and copies bounded entries without exposing process-table storage. The final copy is not fault-contained.',
)

replace_once(
    "docs/ARCHITECTURE.md",
    'ArmoniOS is a compact monolithic AArch64 kernel whose verified development platform is QEMU `virt`. Raspberry Pi code exists only as an experimental future port.',
    'ArmoniOS is a compact monolithic AArch64 kernel whose verified development platform is QEMU `virt`. The Raspberry Pi 4 board layer and read-only EMMC2 diagnostic probe are build/host verified, but physical hardware behavior remains unverified and fail-closed.',
)
replace_once(
    "docs/ARCHITECTURE.md",
    'The syscall helper layer first checks the registered process range, then walks the process page table. Input buffers require valid user-readable leaves; output buffers also require writable leaves and return `ERR_PERM` on read-only pages before writing any byte. `RISK-001` is closed for permission-aware validation, but the helpers still do direct kernel copies rather than fault-contained copy routines. Some internal syscall paths still need cleanup so lower layers operate on kernel-owned buffers instead of validated raw EL0 pointers.',
    'The syscall helper layer first checks the registered process range, then walks the process page table. Input buffers require valid user-readable leaves; output buffers also require writable leaves and return `ERR_PERM` on read-only pages before writing any byte. Syscall entry points now import VFS buffers, path strings, argv, IPC payloads, GUI outputs, and information outputs through kernel-owned temporaries before lower layers operate. The remaining limitation is that the final byte transfer is not fault-contained against an unexpected translation fault.',
)
replace_once(
    "docs/ARCHITECTURE.md",
    'Kernel syscall bodies still dereference validated user virtual addresses directly. Permission validation now rejects known read-only output pages before writes, but the copy path is not fault-contained against unexpected translation faults.',
    'Lower subsystems no longer receive caller-owned EL0 pointers for the covered syscall payloads. The syscall boundary performs bounded copies after range and PTE checks. Those copies are still ordinary EL1 loads/stores and are not recoverable if an unexpected translation fault occurs after validation.',
)
replace_once(
    "docs/ARCHITECTURE.md",
    'Public pointer handling is centralized in `kernel/syscall_helpers.{c,h}`. The helpers provide process-range validation, page-table permission checks, c-string copying, and byte copy helpers. Fault-contained copy is still future work.',
    'Public pointer handling is centralized in `kernel/syscall_helpers.{c,h}`. The helpers provide process-range validation, page-table permission checks, c-string copying, argv import, and checked byte copies. State-consuming outputs validate the whole destination before dequeueing, then perform a bounded final copy. Fault-contained copy is still future work.',
)
replace_once(
    "docs/ARCHITECTURE.md",
    '''The VFS is a fixed-table in-kernel facade over mounted node callbacks. It supports bootfs, tmpfs, and dynamic FAT32 root-file nodes.
''',
    '''The VFS is a fixed-capacity in-kernel facade with static nodes plus a small mount table. Mount callbacks dispatch open, list, unlink, and rename without embedding FAT32 knowledge in the generic layer. It supports bootfs, tmpfs, and dynamic FAT32 root-file nodes.
''',
)
replace_once(
    "docs/ARCHITECTURE.md",
    '''The v1 roadmap requires this layer to become a real storage platform: a
block-device abstraction, mount table, common path resolver, filesystem driver
interface, and structured metadata/directory ABI. Those pieces are planned, not
current architecture.
''',
    '''The current mount table and filesystem callback boundary are intentionally small. The v1 roadmap still requires a common path resolver, structured metadata/directory ABI, richer block-device metadata, and filesystem semantics beyond root-only FAT32.
''',
)
replace_once(
    "docs/ARCHITECTURE.md",
    'It does not claim long-file-name support, subdirectories, arbitrary partition discovery, journaling, crash recovery, or broad compatibility testing.',
    'It does not claim long-file-name support, subdirectories, GPT or extended-partition discovery, journaling, crash recovery, or broad compatibility testing. A reusable MBR parser and bounded block view can locate and validate one supported primary FAT32 partition; that path is currently used by the opt-in RPi4 read-only probe, not exposed as normal writable board storage.',
)
replace_once(
    "docs/ARCHITECTURE.md",
    'The RPi4 board directory is still not a supported hardware implementation. The build-contract milestone (`RISK-006`) is closed; eMMC correctness and physical boot evidence remain under `RISK-007`.',
    'The RPi4 board directory is still not a supported hardware implementation. The SDHCI controller core, firmware clock query, MBR parser, bounded block view, and minimal read-only probe are implemented and host/build verified. Normal board capabilities remain zero and storage stays fail-closed until repeatable physical serial evidence closes `RISK-007`.',
)
replace_once(
    "docs/ARCHITECTURE.md",
    '''1. v0.2 cleanup: kernel-owned syscall buffers, VFS/FAT decoupling, and fail-closed RPi storage behavior;
2. v0.3 storage platform: block devices, mount table, path resolver, and filesystem driver interface;
3. v0.4 real FAT: long names, directories, partition discovery, and persistence tests;
''',
    '''1. finish v0.2 hardening: fault-contained user copies and any remaining syscall-boundary audits;
2. v0.3 storage platform: richer block-device metadata, common path resolution, and structured filesystem ABI;
3. v0.4 real FAT: long names, directories, broader partition handling, and persistence tests;
''',
)

replace_once(
    "docs/TECHNICAL_RISKS.md",
    '| RISK-012 | P1 for v0.2 | Syscall boundary | OPEN | Some syscall internals still rely on validated raw EL0 pointers and non-fault-contained byte copies. |',
    '| RISK-012 | P1 for v0.2 | Syscall buffer ownership | CLOSED | VFS, argv, IPC, GUI, and information syscall payloads cross through bounded kernel-owned temporaries. |\n| RISK-015 | P2 hardening | Fault-contained copy | OPEN | User-copy transfers remain ordinary EL1 loads/stores without exception recovery. |',
)
replace_once(
    "docs/TECHNICAL_RISKS.md",
    '''### RISK-007 — Raspberry Pi eMMC driver is not a valid storage reference

**Severity:** P0 for the hardware track
**Affected scope:** Raspberry Pi hardware support
**Evidence:** static driver inspection

The current eMMC implementation mixes register offsets and block-size constants, uses inconsistent buffer indexing, constructs command indexes through conflicting constants, and marks the controller ready without a complete SD-card initialization sequence.

This file must be treated as experimental scaffolding, not a working driver.

**Exit criteria:** rewrite or validate the controller sequence against BCM2711 documentation; add pure command/register tests where possible; confirm sector reads on physical hardware before enabling FAT writes.
''',
    '''### RISK-007 — Raspberry Pi storage lacks physical evidence

**Severity:** P0 for the hardware track
**Affected scope:** Raspberry Pi hardware support
**Evidence:** controller host tests, build-verified diagnostic image, no physical serial run

The SDHCI controller core, firmware clock query, broken-card-detect adapter, failure telemetry, primary FAT32 MBR discovery, and bounded partition view are implemented. The opt-in image is read-only and the normal RPi4 board still advertises no capabilities.

The remaining blocker is physical evidence, not a claim that the old scaffold is production-ready. No clock response, card initialization, sector read, or FAT geometry has been confirmed on a Raspberry Pi 4.

**Exit criteria:** boot the diagnostic image on real hardware; record repeatable serial telemetry across cold boots; confirm sector-zero and FAT geometry reads; only then consider exposing `BOARD_CAP_STORAGE`. Writes remain a later disposable-media milestone.
''',
)
replace_once(
    "docs/TECHNICAL_RISKS.md",
    '''### RISK-012 — Syscall internals still need kernel-owned copy boundaries

**Severity:** P1 for v0.2
**Affected scope:** syscall implementation, VFS/storage cleanup, process isolation hardening
**Evidence:** static syscall-boundary inspection

The public helper layer now validates user ranges and output PTE write
permissions, closing the v0.1 P0 permission bug. The remaining design debt is
that several syscall paths can still validate an EL0 pointer and then let
deeper code operate through ordinary kernel loads/stores. That keeps the copy
path non-fault-contained and makes future VFS/storage refactors harder to
reason about.

**Exit criteria:** syscall entry points copy path strings, argv, IPC payloads,
I/O buffers, and output structs into kernel-owned temporary buffers before
calling lower layers; helper tests cover invalid, read-only, and boundary-crossing
cases; `bash tools/verify.sh` still passes.
''',
    '''### RISK-015 — User-copy is not fault-contained

**Severity:** P2 hardening
**Affected scope:** syscall exception recovery and hostile/racy address-space changes
**Evidence:** static user-copy inspection

Permission-aware validation and kernel-owned buffer boundaries are implemented. The final byte transfers still use ordinary EL1 loads/stores. ArmoniOS currently has no exception-table or recovery mechanism that can turn an unexpected translation fault during copy into a syscall error.

This is distinct from buffer ownership: lower subsystems no longer operate on caller pointers, but the boundary copy itself is not hardened against a mapping changing unexpectedly after validation.

**Exit criteria:** add fault-recoverable copyin/copyout primitives, targeted exception-path tests, and preserve the current `ERR_INVAL`/`ERR_PERM` contracts without crashing EL1.
''',
)
replace_once(
    "docs/TECHNICAL_RISKS.md",
    '''The current VFS is a fixed-table facade over bootfs, tmpfs, and dynamic FAT32
root nodes. The FAT32 implementation is limited to root-directory 8.3 names and
does not provide long names, subdirectories, partition discovery, a generic
mount table, a common path resolver, structured directory entries, or ext2.
''',
    '''The current VFS now has a small generic mount table and filesystem callbacks, and the storage layer has a reusable primary-MBR FAT32 parser plus bounded block views. FAT32 is still limited to root-directory 8.3 names and does not provide long names, subdirectories, GPT/extended partitions, a common path resolver, structured directory entries, or ext2.
''',
)
replace_once(
    "docs/TECHNICAL_RISKS.md",
    '''**Exit criteria:** the v0.3-v0.4 storage roadmap lands with block-device
metadata, a mount table, common path resolution, filesystem-driver operations,
real FAT long-name/directory support, host image tests, QEMU persistence tests,
and updated syscall documentation.
''',
    '''**Exit criteria:** the v0.3-v0.4 storage roadmap lands with richer block-device metadata, common path resolution, structured filesystem operations, real FAT long-name/directory support, host image tests, QEMU persistence tests, and updated syscall documentation.
''',
)
replace_once(
    "docs/TECHNICAL_RISKS.md",
    '### RISK-013 — Storage and VFS are too narrow for v1',
    '''### RISK-012 — Kernel-owned syscall buffers

VFS buffers and paths, argv, IPC messages, GUI output, and system-information output are copied through bounded kernel-owned temporaries before lower layers use them. Invalid or read-only destinations are rejected before state-consuming receives dequeue data.

**Evidence:** host user-copy boundary regressions plus the complete `tools/verify.sh` matrix recorded with the closing change.

### RISK-013 — Storage and VFS are too narrow for v1''',
)

Path(__file__).unlink()
