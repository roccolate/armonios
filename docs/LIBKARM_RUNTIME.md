# libkarm Runtime Foundation

## Purpose

`libkarm` is the freestanding native runtime between ArmoniOS applications and
the public syscall ABI. It must remain usable without GUI support and must not
hide mutable global state from applications.

The current dependency direction is:

```text
application -> libarmdesk -> libkarm -> public ABI -> kernel
console app -------------> libkarm -> public ABI -> kernel
```

## Arena allocator

`<libkarm/arena.h>` provides an explicit bump arena backed by one anonymous
`SYS_MMAP` region:

```c
kli_arena_t arena;

if (kli_arena_init(&arena, 4096) < 0) {
    return 1;
}

state_t *state = kli_arena_alloc_zero(&arena, sizeof(*state));
if (state == 0) {
    kli_arena_destroy(&arena);
    return 1;
}

/* use state */

kli_arena_destroy(&arena);
```

### Contract

- one arena consumes one user mapping;
- allocations are aligned to 16 bytes;
- allocation is monotonic and cannot overflow the arena capacity;
- `kli_arena_alloc_zero()` explicitly clears the returned bytes;
- `kli_arena_reset()` invalidates all prior arena allocations at once;
- `kli_arena_destroy()` unmaps the backing region and clears the arena handle;
- individual allocations cannot be freed;
- the caller owns the `kli_arena_t` handle and must keep it alive while using
  allocations from the arena;
- no allocator state is stored in `.data`, `.bss`, TLS, or hidden globals.

The allocator stores the requested mapping size. This is valid with the current
`SYS_MMAP`/`SYS_MUNMAP` contract because both operations page-align the supplied
size and unmap requires the same logical region.

## Why an arena first

ArmoniOS currently permits only a small number of registered user mappings per
process. Mapping every small allocation independently would exhaust those
regions quickly. A single arena supports application state, strings, parser
nodes, bytecode, and temporary buffers inside one mapping.

This is a foundation for:

- dynamic strings and byte buffers;
- application contexts;
- widget trees in `libarmdesk`;
- ArmoniBASIC tokenizer, parser, bytecode, and VM state;
- a later `malloc` compatibility layer.

## Non-goals

The first arena is not:

- a general-purpose heap;
- thread-safe;
- a replacement for future `malloc`/`realloc`/`free` compatibility;
- suitable for independently freeing long-lived objects;
- an excuse to add allocator behavior to the kernel.

A later heap can be built over one or more arenas while retaining explicit
ownership and avoiding one `mmap` per object.

## Current consumer

Clock uses one 4 KiB arena for all mutable application state. This replaces its
manual direct `mmap`, demonstrates allocation and cleanup, and keeps the
application independent from allocator internals.
