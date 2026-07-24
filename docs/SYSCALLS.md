# Syscall Reference

> **Implementation update — 2026-07-24:** This document reflects merged v0.3 foundations through structured metadata plus the filesystem-error and `SYS_FSINFO` cut. Use `V03_IMPLEMENTATION_STATUS.md` for the broader storage/VFS checkpoint. Issue #76 remains the manual v0.2 validation and release-record task.

ArmoniOS defines a small non-POSIX syscall ABI for freestanding AArch64 applications.

This document describes the current implementation, including known unsafe or incomplete contracts. Operational verification lives in `CURRENT_STATE.md`; risk history and remaining exit criteria live in `TECHNICAL_RISKS.md`.

## Calling convention

```text
instruction   svc #0
x8            syscall number
x0..x6        arguments
x0            return value; negative values are kernel error codes
```

For a syscall that returns to the same process, the current implementation preserves the saved process frame except for the returned `x0` value.

Syscall numbers are frozen in `kernel/syscall_numbers.h`. Existing numbers and argument layouts must not be reused.

## Important current safety limits

### User pointers cross a kernel-owned boundary

Every syscall pointer is checked against the current process's registered user ranges and page table. This prevents one process from directly passing an address that belongs only to another process.

The helper layer distinguishes:

- readable source memory;
- writable destination memory;
- executable image memory.

`sys_user_buf_in()` requires valid EL0-readable pages. `sys_user_buf_out()` additionally rejects read-only pages with `ERR_PERM` before writing any byte.

Syscall entry points copy input payloads into bounded kernel temporaries before lower layers use them. Output calls first build kernel-owned values. Calls that consume state, such as IPC receive and window-event receive, validate the complete destination before dequeuing data. Most paths use `sys_copy_from_user()` or `sys_copy_to_user()`; already validated state-consuming paths use the shared `kmemcpy()` primitive for the final bounded copy.

The remaining limitation is fault containment: these byte copies are ordinary kernel loads/stores, not exception-recoverable copyin/copyout routines.

### VFS file descriptors are process-local

Descriptors `3+` are local to the current process. The VFS stores owner PID and local fd in an internal global handle pool, validates operations against `process_current()`, lazily reaps dead owners, and closes all descriptors from `process_mark_exited()`.

The current limit is eight open descriptors per process, backed by a fixed global pool sized for all process slots.

## Process syscalls

| # | Name | Arguments | Return | Current behavior |
|---:|---|---|---|---|
| 1 | `sys_exit` | `x0=code` | no normal return | Mark the current process exited and dispatch another ready EL0 process when available. |
| 2 | `sys_yield` | none | 0/current result | Voluntarily dispatch another ready EL0 process when available. |
| 3 | `sys_getpid` | none | PID | Return the current process ID. |
| 4 | `sys_spawn` | `x0=path, x1=entry_index` | PID/error | Load one KLI1 image from VFS and create a ready process. |
| 6 | `sys_wait` | `x0=pid` | exit code/error | Non-blocking: succeeds only when `pid` is a zombie child of the caller, then returns its exit code and reclaims it. Foreign children return `ERR_PERM`. |
| 7 | `sys_kill` | `x0=pid` | 0/error | Mark another live process exited with the kernel kill code. |
| 8 | `sys_spawn_argv` | `x0=path, x1=entry_index, x2=argv_ptr, x3=argc` | PID/error | Import argv into a pointer-free kernel block, then build the child argv on its new stack. |

Current spawn limitations:

- application paths are expected under `/armonios/`;
- entry indexes must exist in the KLI1 header;
- argv is capped at 8 strings and 256 bytes total payload; the loader receives kernel-owned bytes plus offsets, never caller addresses;
- process slots are fixed and capped by `PROCESS_MAX_PROCESSES`;
- the loader uses fixed image and stack virtual slots per process-table index.

## Memory syscalls

| # | Name | Arguments | Return | Current behavior |
|---:|---|---|---|---|
| 20 | `sys_mmap` | `x0=hint, x1=size, x2=flags` | virtual address/error | Allocate contiguous physical pages and map them in the current process. |
| 21 | `sys_munmap` | `x0=address, x1=size` | 0/error | Unmap and free an exact anonymous mapping owned by the current process. |

Protection constants:

```text
0x01  USER_VM_PROT_READ
0x02  USER_VM_PROT_WRITE
0x04  USER_VM_PROT_EXEC
0x10  MAP_SHARED   reserved, rejected
0x20  MAP_FIXED    reserved, rejected
```

Current limitations:

- `hint` must be zero;
- `flags == 0` means readable/writable;
- writable and executable permissions cannot be combined; explicit RWX requests are rejected;
- executable EL0 pages remain privileged-execute-never (PXN) in EL1;
- allocations require contiguous physical pages;
- `munmap` requires an exact region match;
- image and stack mappings cannot be removed with `sys_munmap`;
- process region metadata records ownership but is not yet the permission authority used by user-copy helpers.

## I/O and VFS syscalls

| # | Name | Arguments | Return | Current behavior |
|---:|---|---|---|---|
| 40 | `sys_open` | `x0=path, x1=flags` | fd/error | Open an existing VFS node or dynamically open/create a FAT root file. |
| 41 | `sys_close` | `x0=fd` | 0/error | Close one process-local VFS descriptor. |
| 42 | `sys_read` | `x0=fd, x1=buf, x2=len` | bytes/error | Read one stdin byte or data from a VFS descriptor. |
| 43 | `sys_write` | `x0=fd, x1=buf, x2=len` | bytes/error | Write to UART stdout/stderr or a writable VFS descriptor. |
| 44 | `sys_seek` | `x0=fd, x1=offset, x2=whence` | offset/error | Set absolute file offset; only `whence=0` is supported. |
| 45 | `sys_stat` | `x0=path, x1=stat_ptr` | 0/error | Write one `uint64_t size` result. |
| 46 | `sys_readdir` | `x0=path, x1=buf, x2=len` | bytes/error | Write newline-separated entries for supported list mounts. |
| 47 | `sys_unlink` | `x0=path` | 0/error | Delete a FAT32 root file. |
| 48 | `sys_rename` | `x0=old_path, x1=new_path` | 0/error | Rename a FAT32 root file when the destination does not exist. |
| 49 | `sys_stat_v2` | `x0=path, x1=stat_ptr` | 0/error | Validate a versioned 32-byte output record and return type, size, and generic attributes. |
| 50 | `sys_readdir_v2` | `x0=path, x1=start_index, x2=entries, x3=max_entries` | entry count/error | Return up to eight complete 96-byte versioned directory records, paged by logical index. |
| 51 | `sys_fsinfo` | `x0=path, x1=info_ptr` | 0/error | Return a versioned 64-byte filesystem report for the mount owning the canonical path. |

Descriptor numbers:

```text
0  stdin   shared input queue, non-blocking, at most one byte per call
1  stdout  UART
2  stderr  UART
3+ VFS     process-local fd, maximum 8 open entries per process
```

Open flags:

```text
0x00  O_RDONLY
0x01  O_WRONLY
0x02  O_RDWR
0x40  O_CREAT
```

`O_CREAT` is currently accepted only for `/fat/<valid-8.3-name>`.

FAT32 syscall scope on current `main`:

- existing nested short-8.3 directory trees can be normalized, traversed, listed, statted, opened, and read;
- create, write, unlink, and rename remain limited to root entries;
- long names, mkdir/rmdir, truncate, nested mutation, ext2, and durable reboot semantics remain absent;
- calls 49/50 expose structured metadata and directory entries;
- call 51 reports FAT32 identity, capacity, 512-byte blocks, current name/path limits, directory support, read-only state, and real flush capability; exact free bytes remain unavailable;
- the global ABI identifier remains 1.0; see `VFS_METADATA_ABI.md` and `VFS_FSINFO_ABI.md`.

## Planned filesystem ABI extensions

Structured metadata calls 49/50 and filesystem information call 51 are implemented. Assign every remaining number only when kernel code, wrappers, tests, a real userland consumer, and documentation land together. Append numbers; never reuse an existing slot.

| Planned name | Intended purpose |
|---|---|
| `SYS_MKDIR` | Create a directory on filesystems that support directories. |
| `SYS_RMDIR` | Remove an empty directory with filesystem-specific safety checks. |
| `SYS_TRUNCATE` | Resize a file without rewriting it through root-only FAT behavior. |
| `SYS_FSYNC` | Request durable flush only after transport/filesystem semantics are defined. |

## IPC syscalls

| # | Name | Arguments | Return | Current behavior |
|---:|---|---|---|---|
| 60 | `sys_ipc_send` | `x0=target_pid, x1=buf, x2=len` | bytes/error | Queue one bounded message for a target PID. |
| 61 | `sys_ipc_recv` | `x0=buf, x1=capacity` | bytes/error | Receive the next queued message for the current PID. |

Current limitations:

- fixed global queue;
- fixed maximum message size;
- non-blocking empty/full behavior returns `ERR_AGAIN`;
- receive currently requires the expected fixed capacity;
- send copies the source payload before queueing; receive validates the destination before removing the message; copies remain non-fault-contained.

## Window and cursor syscalls

| # | Name | Summary |
|---:|---|---|
| 70 | `sys_window_create` | Create a process-owned window. |
| 71 | `sys_window_destroy` | Destroy a caller-owned window. |
| 72 | `sys_window_draw_text` | Draw text into a caller-owned window backing buffer. |
| 73 | `sys_window_draw_rect` | Draw a clipped filled rectangle. |
| 74 | `sys_window_event` | Copy queued events to the caller. |
| 75 | `sys_window_set_title` | Set title and title-bar height. |
| 76 | `sys_window_redraw` | Mark desktop redraw work pending. |
| 77 | `sys_window_focus` | Raise and focus any window; intentionally cross-process for the panel. |
| 78 | `sys_window_for_pid` | Find the indexed process-owned window for a PID. |
| 79 | `sys_cursor_set_shape` | Set global arrow/hand cursor hint. |
| 80 | `sys_window_flush` | Add a content-local damage rectangle. |
| 81 | `sys_window_get_bounds` | Copy caller-owned window bounds to user memory. |
| 82 | `sys_window_set_bounds` | Move or resize a caller-owned window. |
| 83 | `sys_window_minimize` | Minimize a caller-owned window. |
| 84 | `sys_window_restore` | Restore and raise a window; intentionally usable by the panel. |
| 85 | `sys_window_state` | Copy minimized/focused state bitmap to user memory. |
| 86 | `sys_cursor_register_region` | Configure one of eight per-window cursor-shape regions. |

`gui_event_t` is a 12-byte ABI structure:

```c
uint32_t type;
int32_t  data1;
int32_t  data2;
```

Event IDs:

```text
1  KEY_PRESS
2  KEY_RELEASE
3  MOUSE_CLICK
4  MOUSE_MOVE
5  RESIZE
6  CLOSE
7  MINIMIZE
8  MAXIMIZE
```

Most mutating window calls enforce `owner_pid == current_pid`. The following are deliberately cross-process presentation operations so the panel can manage application windows:

- `sys_window_focus`;
- `sys_window_for_pid`;
- `sys_window_restore`;
- `sys_window_state`.

Current focus policy: normal application wrappers request focus after successful window creation; the kernel rejects no-focus windows such as docks/taskbars. The focus syscall path is covered by `tools/qemu_focus_test.sh`; the files-to-editor flow has existing manual confirmation from rocco on 2026-07-17.

Window bounds and state are assembled in kernel temporaries before copy-out. `sys_window_event` validates the complete destination before removing the first event, so a read-only or invalid output does not consume input. The final copy is not fault-contained.

## System information syscalls

| # | Name | Arguments | Output |
|---:|---|---|---|
| 100 | `sys_timeinfo` | `x0=info_ptr` | Three `uint64_t`: timer ticks, scheduler ticks, scheduler quantums. |
| 101 | `sys_meminfo` | `x0=info_ptr` | Two `uint64_t`: total PMM pages and free PMM pages. |
| 102 | `sys_proclist` | `x0=entries_ptr, x1=max_entries` | Bounded array of non-zombie process entries. |

A process entry is currently:

```c
uint32_t pid;
uint32_t state;
char     name[16];
```

These calls assemble results in kernel temporaries. `sys_proclist` validates the complete requested array before enumeration and copies bounded entries without exposing process-table storage. The final copy is not fault-contained.

## Error codes

The public names below are ArmoniOS-native status values. Kernel-private `ERR_*` aliases and libkarm `KLI_*` aliases resolve to the same numbers.

| Value | Public name | Meaning |
|---:|---|---|
| -2 | `ARMONIOS_ERR_NOMEM` | Out of memory |
| -3 | `ARMONIOS_ERR_NOENT` | File, mount, or resource not found |
| -5 | `ARMONIOS_ERR_BADF` | Bad descriptor or disallowed descriptor operation |
| -7 | `ARMONIOS_ERR_INVAL` | Invalid argument, layout, path, or user range |
| -11 | `ARMONIOS_ERR_AGAIN` | Non-blocking operation cannot complete yet |
| -13 | `ARMONIOS_ERR_PERM` | Ownership or permission denied |
| -14 | `ARMONIOS_ERR_EXIST` | Destination or resource already exists |
| -15 | `ARMONIOS_ERR_NOTDIR` | A required path component is not a directory |
| -16 | `ARMONIOS_ERR_ISDIR` | A regular-file operation received a directory |
| -17 | `ARMONIOS_ERR_NOTEMPTY` | A directory cannot be removed because it contains entries |
| -18 | `ARMONIOS_ERR_NOSPC` | The filesystem or fixed-capacity object has no remaining space |
| -19 | `ARMONIOS_ERR_ROFS` | The selected filesystem is read-only for the requested mutation |
| -20 | `ARMONIOS_ERR_NOTSUP` | The selected mount does not implement the requested operation |
| -21 | `ARMONIOS_ERR_RANGE` | A value or filesystem report cannot be represented safely |

`SYS_FSINFO` already preserves `NOENT`, `NOTSUP`, and `RANGE` from the VFS. Some older open/read/write/mutation paths still collapse filesystem failures to broader legacy errors; later cuts should narrow those results only when the backend can prove the specific condition.

## ABI change requirements

A change to any of the following must update code, wrappers, tests, and this document in the same change:

- syscall number;
- argument register assignment;
- return layout;
- user-visible struct size or field order;
- event ID;
- flag value;
- descriptor semantics;
- pointer-validation contract.

When a syscall is present in code but its safety or ownership contract is incomplete, document the limitation rather than describing it as stable.
