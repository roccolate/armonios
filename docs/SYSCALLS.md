# Syscall Reference

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
| 6 | `sys_wait` | `x0=pid` | exit code/error | Non-blocking: succeeds only for an existing zombie process, then reclaims it. |
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

FAT32 syscall scope:

- root directory only;
- short 8.3 names only;
- create, read, write, seek, rename, delete, stat, and list;
- no subdirectories, long-file-name ABI, partition discovery, ext2 mount, or structured directory entries.

## Planned filesystem ABI extensions

These calls are part of the v1 storage roadmap and are **not implemented** in
the current syscall table. Assign numbers only when the matching kernel,
userland wrappers, tests, and documentation land together. Append new numbers;
do not reuse existing syscall slots.

| Planned name | Intended purpose |
|---|---|
| `SYS_MKDIR` | Create a directory on filesystems that support directories. |
| `SYS_TRUNCATE` | Resize a file without rewriting it through root-only FAT behavior. |
| `SYS_STATX` | Return structured metadata such as type, size, flags, and mount state. |
| `SYS_READDIRX` | Return structured directory entries instead of newline-separated names. |
| `SYS_FSINFO` | Report filesystem type, read-only state, and capacity when available. |

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

| Value | Name | Meaning |
|---:|---|---|
| -2 | `USER_VM_ERR_NOMEM` | Out of memory |
| -3 | `ERR_NOENT` | File or resource not found |
| -5 | `ERR_BADF` | Bad descriptor or disallowed descriptor operation |
| -7 | `ERR_INVAL` | Invalid argument or invalid user range |
| -11 | `ERR_AGAIN` | Non-blocking operation cannot complete yet |
| -13 | `ERR_PERM` | Ownership or permission denied |

Error meanings are still coarse. Several filesystem failures collapse to `ERR_NOENT` or `ERR_BADF`.

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
