# Syscall Reference

ArmoniOS defines its own syscall ABI. There is no POSIX compatibility.

This document separates the syscalls implemented by the current kernel from
numbers reserved for the planned ABI.

## Calling Convention

```
Instruction:  svc #0
Syscall #:    x8
Arguments:    x0, x1, x2, x3, x4, x5, x6  (up to 7)
Return value: x0  (negative = error code)
```

For a syscall that returns to the same process, all registers except `x0` are
preserved. `x0` carries the return value.

`x7` is caller-saved scratch under the AArch64 procedure-call ABI, but it is
not an ArmoniOS syscall argument register today.

---

## Implemented Now

These numbers are handled by `kernel/syscall.c` today. Unknown syscall numbers
return `ERR_INVAL`.

### Process

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 1 | `sys_exit` | `x0=code` | — | Terminate current process |
| 2 | `sys_yield` | — | — | Voluntarily yield CPU slice |
| 3 | `sys_getpid` | — | PID | Return current process ID |
| 4 | `sys_spawn` | `x0=path_ptr, x1=entry_index` | PID / error | Spawn a flat image entry from VFS |
| 6 | `sys_wait` | `x0=pid` | exit code / error | Reclaim an exited process |
| 7 | `sys_kill` | `x0=pid` | 0 / error | Terminate another process |
| 8 | `sys_spawn_argv` | `x0=path, x1=entry_index, x2=argv_ptr, x3=argc` | PID / error | Spawn a flat image and pass argv to it |

Notes:
- `sys_exit` marks the process as exited and switches to the next runnable EL0
  process when one exists.
- `sys_yield` switches to the next runnable EL0 process when one exists.
- `sys_spawn` loads a KLI1 flat image through VFS, starts the selected entry as
  a READY process, and reclaims exited processes before allocating a slot.
- `sys_spawn_argv` extends `sys_spawn` with an argv vector. `argv_ptr` must
  point at `argc` `uint64_t` entries inside the caller's registered user
  regions. The kernel copies each referenced string onto the new process's
  stack, sets `x0 = argc` and `x1 = &argv[0]` in the new process's initial
  frame, and points the initial `sp` at `argv[0]`. The strings and argv array
  survive even after the caller returns because they live in the spawned
  process's own stack memory. Cap: 8 strings, 256 bytes total payload.
- `sys_wait` is non-blocking today: it succeeds only when the target process is
  already `ZOMBIE`; otherwise it returns `ERR_AGAIN`.
- `sys_kill` marks another process exited with
  `KERNEL_USER_KILL_EXIT_CODE` (`0x80`). It currently rejects killing the
  calling process and already-exited processes.

### Memory

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 20 | `sys_mmap` | `x0=hint, x1=size, x2=flags` | vaddr / error | Map anonymous user pages |
| 21 | `sys_munmap` | `x0=vaddr, x1=size` | 0 / error | Unmap owned anonymous user pages |

Current limitations:
- `sys_mmap` allocates contiguous physical pages, installs user PTEs in the
  current process page table, and records process-owned metadata.
- `hint` must be `0`.
- `flags=0` maps readable/writable anonymous memory.
- `USER_VM_PROT_READ`, `USER_VM_PROT_WRITE`, and `USER_VM_PROT_EXEC` are
  supported. `MAP_SHARED` and `MAP_FIXED` are reserved but rejected today.
- `sys_munmap` requires an exact owned `sys_mmap` region match. Image and stack
  regions cannot be unmapped through this syscall yet.

`mmap` protection/map flags (`kernel/user_vm.h`):
```
0x01  USER_VM_PROT_READ
0x02  USER_VM_PROT_WRITE
0x04  USER_VM_PROT_EXEC
0x10  MAP_SHARED        reserved, not implemented
0x20  MAP_FIXED         reserved, not implemented
```

### I/O

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 40 | `sys_open` | `x0=path, x1=flags` | fd / error | Open a VFS file |
| 41 | `sys_close` | `x0=fd` | 0 / error | Close a VFS file descriptor |
| 42 | `sys_read` | `x0=fd, x1=buf, x2=len` | bytes read / error | Read from stdin or a VFS file |
| 43 | `sys_write` | `x0=fd, x1=buf, x2=len` | bytes written / error | Write to UART-backed stdout/stderr or a VFS file |
| 44 | `sys_seek` | `x0=fd, x1=offset, x2=whence` | new offset / error | Seek within a VFS file |
| 45 | `sys_stat` | `x0=path, x1=stat_ptr` | 0 / error | Get VFS file metadata |
| 46 | `sys_readdir` | `x0=path, x1=buf, x2=len` | bytes written / error | List mounted VFS paths or supported VFS directories |
| 47 | `sys_unlink` | `x0=path_ptr` | 0 / error | Remove a file from the underlying filesystem |
| 48 | `sys_rename` | `x0=old_ptr, x1=new_ptr` | 0 / error | Rename a file in the same directory |

Current file descriptors:
```
0  stdin   UART, non-blocking
1  stdout  UART
2  stderr  UART
3+ VFS files
```

Notes:
- `sys_read(0, buf, len)` reads at most one UART byte. It returns `ERR_AGAIN`
  when no byte is available.
- `sys_open` accepts access mode bits `0` (`O_RDONLY`), `1` (`O_WRONLY`),
  and `2` (`O_RDWR`). It also accepts `O_CREAT` (`0x40`) combined with one
  access mode. `O_CREAT` is currently supported only for `/fat/<8.3-name>`
  root files; other paths reject create.
- `sys_read`, `sys_write`, `sys_seek`, and `sys_close` on VFS file descriptors
  are backed by the fixed kernel VFS descriptor table.
- `sys_seek` currently supports only `whence=0` (`SEEK_SET`).
- `sys_stat` writes the current `vfs_stat_t` layout: one `uint64_t size`.
- `sys_readdir("/")` writes newline-separated mounted VFS paths. When FAT32 is
  present, `sys_readdir("/fat")` writes newline-separated root 8.3 directory
  entries.
- `sys_unlink` and `sys_rename` route through `vfs_unlink` / `vfs_rename`.
  Only paths under the FAT32 root mounted at `/fat/` are accepted today;
  the caller's pointers must point to null-terminated user strings inside
  registered user regions. `sys_rename` rejects any destination that
  already exists in the same directory.
- `sys_write` returns `ERR_BADF` for unsupported descriptors or descriptors
  not opened for writing.
- `sys_write` validates that `buf..buf+len` is inside the current process's
  registered user regions before reading from it.
- FAT32 supports root 8.3 file read/write, create, delete, rename, and cluster
  chain growth for the current simple VFS integration.

### IPC

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 60 | `sys_ipc_send` | `x0=target_pid, x1=buf, x2=len` | bytes sent / error | Send one fixed-size queued message |
| 61 | `sys_ipc_recv` | `x0=buf, x1=capacity` | bytes received / error | Receive the next queued message for the caller |

Current limitations:
- Message size is capped at `IPC_MAX_MESSAGE_SIZE`.
- `sys_ipc_recv` requires `capacity == IPC_MAX_MESSAGE_SIZE`.
- The queue is fixed-size and non-blocking; empty/full paths return
  `ERR_AGAIN`.

### Window System

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 70 | `sys_window_create` | `x0=x, x1=y, x2=w, x3=h, x4=bg, x5=border, x6=title_ptr` | window id / error | Create a process-owned window |
| 71 | `sys_window_destroy` | `x0=window_id` | 0 / error | Destroy a window owned by the caller |
| 72 | `sys_window_draw_text` | `x0=window_id, x1=x, x2=y, x3=color, x4=str_ptr` | 0 / error | Draw text inside a window |
| 73 | `sys_window_draw_rect` | `x0=window_id, x1=x, x2=y, x3=w, x4=h, x5=color` | 0 / error | Draw a clipped filled rectangle inside a window |
| 74 | `sys_window_event` | `x0=window_id, x1=buf, x2=max_events` | event count / error | Read queued window events |
| 75 | `sys_window_set_title` | `x0=window_id, x1=title_ptr, x2=title_h` | 0 / error | Replace a window title and optional kernel-drawn title bar height |
| 76 | `sys_window_redraw` | `x0=window_id` | 0 / error | Mark the GUI desktop dirty |
| 77 | `sys_window_focus` | `x0=window_id` | 0 / error | Raise/focus a window by id (callable from any pid, not just the owner) |
| 78 | `sys_window_for_pid` | `x0=owner_pid, x1=index` | window id / `ERR_NOENT` | Return the index-th window owned by the given pid, or `ERR_NOENT` when none |
| 79 | `sys_cursor_set_shape` | `x0=shape` | 0 / error | Request the global cursor shape (`0=arrow`, `1=hand`) |
| 80 | `sys_window_flush` | `x0=window_id, x1=x, x2=y, x3=w, x4=h` | 0 / error | Push a content-local damage rect; the compositor partial-redraws only that region |
| 81 | `sys_window_get_bounds` | `x0=window_id, x1=out_ptr` | 0 / error | Owner-only: copy the window's `(x, y, w, h)` into the caller's 16-byte buffer as four `uint32_t` values |
| 82 | `sys_window_set_bounds` | `x0=window_id, x1=x, x2=y, x3=w, x4=h` | 0 / error | Move and/or resize the window in one step; if `(w, h)` changes the kernel reallocates the per-window backing and pushes `GUI_EVENT_RESIZE` onto the owner's event queue |
| 83 | `sys_window_minimize` | `x0=window_id` | 0 / error | Owner-only: hide the window through the same path the kernel-drawn minimize button uses; pushes `GUI_EVENT_MINIMIZE` onto the owner's queue |
| 84 | `sys_window_restore` | `x0=window_id` | 0 / error | Presentation operation: clears the hidden flag, raises the window, and pushes `GUI_EVENT_MAXIMIZE`; callable by the panel for taskbar restore |
| 85 | `sys_window_state` | `x0=window_id, x1=out_ptr` | 0 / error | Read-only presentation query: write a 32-bit state bitmap into the caller's buffer; bit 0 = minimized, bit 1 = currently focused |
| 86 | `sys_cursor_register_region` | `x0=window_id, x1=slot, x2=x, x3=y, x4=w, x5=h, x6=shape` | 0 / error | Owner-only: install or replace a per-window cursor-shape region. Slot 0..7; the kernel walks the slots in ascending order during cursor refresh and the first region whose content-local rect contains the cursor wins over the kernel title-bar default. Pass `x6=0xffffffff` to clear the slot. |

Current limitations:
- The implemented desktop ABI is frozen for this milestone. New desktop calls
  must be appended with new syscall numbers; existing numbers and argument
  registers must not be reused.
- Window ownership is enforced with the current process pid for draw,
  destroy, get/set bounds, minimize, set-title, event reads, flush, and cursor
  regions. `sys_window_focus`, `sys_window_restore`, `sys_window_state`, and
  `sys_window_for_pid` are deliberately callable from any pid so the desktop
  taskbar (a different pid from the apps it tracks) can draw minimized state,
  restore minimized windows, and raise their windows.
- `sys_window_event` writes packed `gui_event_t` triples:
  `uint32_t type, int32_t data1, int32_t data2`.
- `sys_window_event` yields for a bounded number of scheduler turns and returns
  `ERR_AGAIN` when no event arrives.
- A title-bar close click queues `GUI_EVENT_CLOSE` for a live owner. If the
  owner process has already exited or been reclaimed, the kernel destroys the
  window directly instead.
- `sys_window_set_title` accepts an optional `x2=title_h`. `title_h` of 0 keeps
  the legacy no-title-bar behaviour (default for apps that pre-date this
  argument). Non-zero `title_h` asks the kernel to paint a solid bar at the top
  of the window and draw the title text inside it. The kernel rejects
  `title_h >= window->h` with `ERR_INVAL`. Owner drawing through
  `SYS_WINDOW_DRAW_RECT/TEXT` is shifted down by `title_h` so apps keep a
  0-based content coordinate space below the bar.
- `sys_window_focus` updates the focus border and raises the target window
  above other windows by bumping its z-order. Window ids remain stable pool
  indices; z-order changes do not move window structs.
- `sys_window_for_pid` skips ownerless windows (those whose
  `owner_pid == GUI_NO_OWNER`); only windows actually owned by a process
  are visible through it.
- `sys_cursor_set_shape` is the first app-owned control hint. The kernel
  updates cursor shape itself over kernel title decorations; EL0-drawn
  controls such as panel launcher buttons can request `GUI_CURSOR_HAND`
  while hovered and `GUI_CURSOR_ARROW` when the hover leaves.
- `sys_cursor_register_region` is the per-window refinement of
  `sys_cursor_set_shape`. Apps register up to 8 content-local rectangles,
  each tagged with a shape; the kernel walks the slots in ascending
  index order during cursor refresh and uses the first region whose
  rect contains the cursor. Coords are content-local: the kernel adds
  the window's `(x, y + title_h)` when checking containment. A
  non-owner pid returns `ERR_PERM`. The cap is 8 regions per window;
  once a slot is full, the app must reuse a slot index.
- Drawing writes through per-window backing buffers; the compositor walks
  the per-framebuffer damage list on each redraw and only repaints the
  rectangles the window funcs (or `sys_window_flush`) have queued. Owner
  draws land in the backing buffer; a `sys_window_flush` call is only
  needed for content the owner mutated through a kernel-bypassing path
  (e.g. by mapping the backing into another process).

### System Info

| # | Name | Args | Returns | Description |
|---|------|------|---------|-------------|
| 100 | `sys_timeinfo` | `x0=info_ptr` | 0 / error | Write three `uint64_t` values: timer ticks, scheduler ticks, scheduler quantums |
| 101 | `sys_meminfo` | `x0=info_ptr` | 0 / error | Write two `uint64_t` values: total PMM pages, free PMM pages |
| 102 | `sys_proclist` | `x0=entries_ptr, x1=max_entries` | entries written / error | Write a bounded array of process entries for monitor/shell tooling |

Current limitations:
- `sys_timeinfo` validates a writable user buffer of `3 * sizeof(uint64_t)` and
  writes `timer_ticks()`, `sched_ticks()`, and `sched_quantums()` in that order.
- `sys_meminfo` validates a writable user buffer of `2 * sizeof(uint64_t)` and
  writes `pmm_total_count()` and `pmm_free_count()` in that order.
- `sys_proclist` accepts `max_entries == 0` and returns 0 without touching the
  buffer. Non-zero `max_entries` must be no larger than
  `PROCESS_MAX_PROCESSES`.
- Each `sys_proclist` entry is currently 24 bytes:
  `uint32_t pid`, `uint32_t state`, and `char name[16]`. Process names are
  copied from the kernel process slot and always null-terminated.
- The stable userland wrappers for these calls live in `programs/libkarm` as
  `kli_timeinfo`, `kli_meminfo`, and `kli_proclist`.

### Error Codes Implemented Today

| Code | Name | Meaning |
|------|------|---------|
| -2 | `USER_VM_ERR_NOMEM` | Out of memory |
| -3 | `ERR_NOENT` | File or resource not found |
| -5 | `ERR_BADF` | Bad file descriptor |
| -7 | `ERR_INVAL` | Invalid argument |
| -11 | `ERR_AGAIN` | Try again later |
| -13 | `ERR_PERM` | Permission denied |

### Memory Isolation Contract

Every syscall that takes a user pointer validates the caller's registered
user regions before reading or writing it. The contract is enforced by
the helper layer in `kernel/syscall_helpers.{c,h}` and pinned by
`tests/test_syscall_abi.c` and `tests/test_process_isolation.c`.

- Each process owns up to `PROCESS_MAX_USER_REGIONS` (`kernel/process.h`,
  currently 8) disjoint regions.
- The kernel's user-region list is per-process. A region registered by
  process A is invisible to `process_user_range_contains` when called on
  process B.
- `sys_mmap` and `sys_munmap` operate only on the current process's
  regions. Each process's `next_user_vaddr` is private, so two processes
  can both receive the same virtual address from `sys_mmap` and the
  pages they back live in independent page tables.
- Image and stack regions are added by the loader and are not user-mutable
  through `sys_mmap`/`sys_munmap`.
- A user pointer that crosses a region boundary is rejected with
  `ERR_INVAL`. The boundary check treats `size == 0` as a vacuous query
  that is always satisfied; this matters for callers that validate a
  pointer before computing a length.
- A null `process_t` pointer never satisfies the range check.

### GUI / Window-Manager ABI

The kernel exposes the desktop as a per-process set of windows. The
contract below is enforced by the `kernel/gui_*` modules and pinned by
`tests/test_window_abi.c`.

- Every window carries one `owner_pid`. `gui_window_t.owner_pid ==
  GUI_NO_OWNER` (`0xffffffff`) marks a kernel-owned window and is the
  only owner value `sys_window_for_pid` skips.
- `sys_window_create`, `sys_window_destroy`, draw, title, event, bounds,
  minimize, flush, and cursor-region operations are owner-only.
- `sys_window_focus`, `sys_window_restore`, `sys_window_state`, and
  `sys_window_for_pid` are intentionally callable from any pid. The panel
  taskbar uses them to draw state, restore, and raise a different pid's
  window.
- `gui_event_t` is a packed 12-byte triple: `uint32_t type,
  int32_t data1, int32_t data2` (4 bytes each). The width and the
  order are part of the ABI; apps size their event buffers as
  `count * sizeof(gui_event_t)`.
- Event type IDs are frozen: `1=KEY_PRESS, 2=KEY_RELEASE,
  3=MOUSE_CLICK, 4=MOUSE_MOVE, 5=RESIZE, 6=CLOSE, 7=MINIMIZE,
  8=MAXIMIZE`. Reordering them
  silently breaks every windowed app.
- `sys_window_focus` raises a window by bumping its `z` field. Window
  pool indices stay stable; z-order changes never move window structs.
- `GUI_WINDOW_NO_FOCUS` keeps the focus machinery from selecting the
  window. Dock/taskbar windows set this.
- `GUI_WINDOW_NO_DRAG` and `GUI_WINDOW_SKIP_TASKBAR` are reserved.
