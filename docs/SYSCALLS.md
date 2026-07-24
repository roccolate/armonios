# Syscall reference

ArmoniOS exposes a small, native, non-POSIX syscall ABI to freestanding AArch64
applications. Public numbers, status values, flags, and structure layouts come
from `include/armonios/abi/`; kernel-private compatibility headers are not the
source of truth.

Related documents:

- ABI ownership and compatibility: `PUBLIC_ABI.md`;
- implemented system boundaries: `ARCHITECTURE.md`;
- current capability summary: `CURRENT_STATE.md`;
- userland wrappers and runtime helpers: `LIBKARM.md`.

## Calling convention

```text
instruction   svc #0
x8            syscall number
x0..x6        arguments
x0            result or negative ArmoniOS status
```

For a syscall that returns to the same process, the saved frame is preserved
except for the returned `x0` value.

Existing syscall numbers are append-only. A retired number must not be reused,
because independently built KLI1 applications may remain on disk.

## User-memory boundary

Syscall entry points validate user ranges and page permissions before accessing
EL0 memory.

The current boundary distinguishes:

- readable input memory;
- writable output memory;
- executable image memory.

Input payloads are imported into bounded kernel-owned storage before lower
subsystems use them. Output payloads are assembled in kernel-owned storage and
copied only after the complete destination has been validated. State-consuming
operations validate output before dequeuing or consuming kernel state.

The remaining limitation is fault containment: final copies use ordinary EL1
loads and stores. A late translation fault is not yet converted into a recoverable
syscall error.

## Standard descriptors

```text
0  stdin   shared non-blocking input stream
1  stdout  UART
2  stderr  UART
3+ VFS     process-local descriptor
```

Descriptors `3+` are owned by one process. The current limit is eight open VFS
descriptors per process.

## Process calls

| Number | Public name | Arguments | Result | Contract |
|---:|---|---|---|---|
| 1 | `SYS_EXIT` | `x0=code` | no normal return | Mark the current process exited and dispatch another ready process. |
| 2 | `SYS_YIELD` | none | status | Voluntarily dispatch another ready process when available. |
| 3 | `SYS_GETPID` | none | PID | Return the current process identifier. |
| 4 | `SYS_SPAWN` | `x0=path, x1=entry_index` | PID/status | Load a KLI1 entry from VFS and create a process. |
| 6 | `SYS_WAIT` | `x0=pid` | exit code/status | Non-blocking wait for a zombie child, followed by reclamation. |
| 7 | `SYS_KILL` | `x0=pid` | status | Exit another live process using the kernel kill result. |
| 8 | `SYS_SPAWN_ARGV` | `x0=path, x1=entry_index, x2=argv, x3=argc` | PID/status | Import bounded arguments and create a child stack. |

Current spawn boundaries:

- application images are normally loaded from `/armonios`;
- `argv` is limited to eight strings and 256 bytes of imported text;
- process slots and user-region tables are fixed-capacity;
- no executable loader for arbitrary external formats exists.

## Memory calls

| Number | Public name | Arguments | Result | Contract |
|---:|---|---|---|---|
| 20 | `SYS_MMAP` | `x0=hint, x1=size, x2=flags` | address/status | Allocate contiguous pages and create an anonymous user mapping. |
| 21 | `SYS_MUNMAP` | `x0=address, x1=size` | status | Remove one exact anonymous mapping owned by the process. |

Protection bits:

```text
0x01  ARM_PROT_READ
0x02  ARM_PROT_WRITE
0x04  ARM_PROT_EXEC
0x10  ARM_MAP_SHARED   reserved and rejected
0x20  ARM_MAP_FIXED    reserved and rejected
```

Current boundaries:

- `hint` must be zero;
- flags zero means readable and writable;
- writable and executable permissions cannot be combined;
- allocations require contiguous physical pages;
- `SYS_MUNMAP` requires an exact region match;
- process images and stacks cannot be removed through `SYS_MUNMAP`.

## VFS calls

| Number | Public name | Arguments | Result | Contract |
|---:|---|---|---|---|
| 40 | `SYS_OPEN` | `x0=path, x1=flags` | fd/status | Open an existing node or create a supported FAT32 root file. |
| 41 | `SYS_CLOSE` | `x0=fd` | status | Close one process-local descriptor. |
| 42 | `SYS_READ` | `x0=fd, x1=buffer, x2=size` | bytes/status | Read stdin or a VFS descriptor. |
| 43 | `SYS_WRITE` | `x0=fd, x1=buffer, x2=size` | bytes/status | Write UART output or a writable VFS descriptor. |
| 44 | `SYS_SEEK` | `x0=fd, x1=offset, x2=whence` | offset/status | Change descriptor position; only absolute seek is complete. |
| 45 | `SYS_STAT` | `x0=path, x1=arm_stat_t*` | status | Legacy eight-byte size record. |
| 46 | `SYS_READDIR` | `x0=path, x1=buffer, x2=size` | bytes/status | Legacy newline-separated directory listing. |
| 47 | `SYS_UNLINK` | `x0=path` | status | Delete a supported FAT32 root file. |
| 48 | `SYS_RENAME` | `x0=old_path, x1=new_path` | status | Rename a supported FAT32 root file. |
| 49 | `SYS_STAT_V2` | `x0=path, x1=arm_stat_v2_t*` | status | Return versioned type, size, and generic attributes. |
| 50 | `SYS_READDIR_V2` | `x0=path, x1=start, x2=entries, x3=count` | entry count/status | Return versioned directory records by logical index. |
| 51 | `SYS_FSINFO` | `x0=path, x1=arm_fsinfo_t*` | status | Report capabilities and limits for the owning mount. |

Open flags:

```text
0x00  ARM_O_RDONLY
0x01  ARM_O_WRONLY
0x02  ARM_O_RDWR
0x40  ARM_O_CREAT
```

Structured records:

```text
arm_stat_t       8 bytes   legacy size-only payload
arm_stat_v2_t   32 bytes   versioned file metadata
arm_dirent_v2_t 96 bytes   versioned directory entry
arm_fsinfo_t    64 bytes   versioned filesystem information
```

The caller initializes the required version and structure-size fields for the
versioned records. The kernel validates the complete destination before invoking
the provider and copies a kernel-owned result to EL0.

Current FAT32 scope:

- canonical absolute paths and longest-prefix mount selection are implemented;
- existing nested 8.3 directories can be traversed, listed, statted, opened, and
  read;
- create, write, unlink, and rename remain root-entry operations;
- long names, mkdir/rmdir, truncate, nested mutation, and reboot-verified
  durability are not implemented;
- `SYS_FSINFO` reports identity, capacity, block size, name/path limits,
  directory support, read-only state, and whether a real flush callback exists;
- exact free-byte accounting is not advertised.

Planned VFS additions do not have assigned numbers until implementation, wrappers,
tests, a real consumer, and documentation land together:

- directory creation and removal;
- truncate or descriptor-based resize;
- explicit durable flush/fsync semantics.

## IPC calls

| Number | Public name | Arguments | Result | Contract |
|---:|---|---|---|---|
| 60 | `SYS_IPC_SEND` | `x0=pid, x1=buffer, x2=size` | bytes/status | Copy and queue one bounded message. |
| 61 | `SYS_IPC_RECV` | `x0=buffer, x1=capacity` | bytes/status | Copy the next message for the current process. |

IPC uses fixed-capacity storage and non-blocking empty/full behavior. Receive
validates the entire destination before removing a message.

## Window and cursor calls

| Number | Public name | Contract |
|---:|---|---|
| 70 | `SYS_WINDOW_CREATE` | Create a process-owned window. |
| 71 | `SYS_WINDOW_DESTROY` | Destroy a caller-owned window. |
| 72 | `SYS_WINDOW_DRAW_TEXT` | Draw text into a window backing buffer. |
| 73 | `SYS_WINDOW_DRAW_RECT` | Draw a clipped filled rectangle. |
| 74 | `SYS_WINDOW_EVENT` | Copy one queued event to userland. |
| 75 | `SYS_WINDOW_SET_TITLE` | Set title text and title-bar height. |
| 76 | `SYS_WINDOW_REDRAW` | Publish desktop redraw work. |
| 77 | `SYS_WINDOW_FOCUS` | Raise and focus a window. |
| 78 | `SYS_WINDOW_FOR_PID` | Find a process-owned window by index. |
| 79 | `SYS_CURSOR_SET_SHAPE` | Set the global cursor hint. |
| 80 | `SYS_WINDOW_FLUSH` | Add a content-local damage rectangle. |
| 81 | `SYS_WINDOW_GET_BOUNDS` | Return caller-owned window bounds. |
| 82 | `SYS_WINDOW_SET_BOUNDS` | Move or resize a caller-owned window. |
| 83 | `SYS_WINDOW_MINIMIZE` | Minimize a caller-owned window. |
| 84 | `SYS_WINDOW_RESTORE` | Restore and raise a window. |
| 85 | `SYS_WINDOW_STATE` | Return minimized/focused state bits. |
| 86 | `SYS_CURSOR_REGISTER_REGION` | Configure one of eight per-window cursor regions. |

`gui_event_t` is a frozen 12-byte public record:

```c
uint32_t type;
int32_t  data1;
int32_t  data2;
```

Event values and GUI flags are defined in `include/armonios/abi/gui.h`.

Most mutating window calls require ownership by the current process. Focus,
window discovery, restore, and state inspection intentionally permit the panel to
manage application windows. This authority model is suitable only for the current
trusted, compiled-in application set.

## System-information calls

| Number | Public name | Output |
|---:|---|---|
| 100 | `SYS_TIMEINFO` | `arm_timeinfo_t`, 24 bytes |
| 101 | `SYS_MEMINFO` | `arm_meminfo_t`, 16 bytes |
| 102 | `SYS_PROCLIST` | bounded `arm_process_entry_t` array, 24 bytes per entry |

These calls construct bounded kernel-owned snapshots. The process-list call
validates the requested destination range before enumeration.

## Public status values

| Value | Public name | Meaning |
|---:|---|---|
| -2 | `ARMONIOS_ERR_NOMEM` | Memory or fixed-capacity allocation failed. |
| -3 | `ARMONIOS_ERR_NOENT` | Resource or owning mount was not found. |
| -5 | `ARMONIOS_ERR_BADF` | Descriptor is invalid for the operation. |
| -7 | `ARMONIOS_ERR_INVAL` | Argument, layout, path, or user range is invalid. |
| -11 | `ARMONIOS_ERR_AGAIN` | Non-blocking operation cannot complete yet. |
| -13 | `ARMONIOS_ERR_PERM` | Ownership or permission denied. |
| -14 | `ARMONIOS_ERR_EXIST` | Destination already exists. |
| -15 | `ARMONIOS_ERR_NOTDIR` | A required path component is not a directory. |
| -16 | `ARMONIOS_ERR_ISDIR` | A regular-file operation received a directory. |
| -17 | `ARMONIOS_ERR_NOTEMPTY` | A directory is not empty. |
| -18 | `ARMONIOS_ERR_NOSPC` | Capacity is exhausted. |
| -19 | `ARMONIOS_ERR_ROFS` | The selected filesystem is read-only. |
| -20 | `ARMONIOS_ERR_NOTSUP` | The provider does not implement the operation. |
| -21 | `ARMONIOS_ERR_RANGE` | A value cannot be represented safely. |

Older VFS operations may still collapse backend failures into broader legacy
statuses. New operations should return the most specific result the provider can
prove without changing frozen legacy behavior silently.

## Change requirements

A public syscall change must update together:

- the public ABI header;
- kernel dispatch and implementation;
- `libkarm` or `libarmdesk` wrappers;
- layout and compatibility assertions;
- focused host tests;
- QEMU evidence when behavior changes;
- this reference and any affected current-state or architecture text.
