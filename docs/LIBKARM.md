# libkarm

`libkarm` is the freestanding base runtime for ArmoniOS applications. It sits
above the public syscall ABI and below the optional desktop layer.

```text
application -> libarmdesk -> libkarm -> public ABI -> kernel
console app -------------> libkarm -> public ABI -> kernel
```

`libkarm` must remain independent from GUI and desktop policy. `libarmdesk` may
depend on it.

## Build products

The userland build produces:

```text
build/programs/libkarm/crt0.o
build/programs/libkarm/libkarm.a
```

### `crt0.o`

`crt0.o` owns `_start`. It receives the loader-provided argument state, calls
`main(argc, argv)`, and exits through `SYS_EXIT`.

It stays outside the archive so startup is always linked explicitly.

### `libkarm.a`

The static archive contains reusable runtime members such as:

```text
syscall.o
io.o
string.o
arena.o
arena_map.o
buffer.o
file.o
```

GNU `ld` extracts only referenced members. Function/data sections and
`--gc-sections` remove unused functions from extracted members, so adding a module
to the archive does not automatically add all of it to every KLI1 application.

The effective application link shape is:

```text
application.o
application_header.o
crt0.o
libkarm.a
application_end.o
```

The KLI1 header, end marker, and linker script remain executable-format concerns;
they are not owned by `libkarm.a`.

## Syscall and basic-runtime layer

The base archive provides:

- syscall trampolines using public numbers and records;
- minimal UART/stdout helpers;
- byte and C-string utilities suitable for freestanding code;
- integer conversion;
- typed wrappers for process, memory, VFS, GUI-independent system information,
  and structured filesystem calls.

Public layouts come from `include/armonios/abi/`. Runtime modules must not copy
kernel-private structures or syscall values.

## Monotonic arena

`kli_arena_t` is an explicit caller-owned allocator.

```c
kli_arena_t arena;

if (kli_arena_map(&arena, 64 * 1024) < 0) {
    return 1;
}

void *tokens = kli_arena_alloc(&arena, token_bytes);
void *nodes = kli_arena_alloc_aligned(&arena, node_bytes, 16);

kli_arena_reset(&arena);
(void)kli_arena_destroy(&arena);
```

An arena may use caller-provided storage through `kli_arena_init`, or one
`SYS_MMAP` region through `kli_arena_map`.

Contracts:

- no hidden global allocator state;
- overflow-checked offset and alignment arithmetic;
- custom alignment must be a power of two;
- failed allocation does not consume capacity;
- reset invalidates all allocations in constant time;
- destroy uses the exact mapped range with `SYS_MUNMAP`;
- failed unmap preserves state for retry or diagnosis.

This is not `malloc`/`free`. Individual allocations cannot be released, and
objects backed by an arena become invalid after reset or destroy.

## Binary buffers

`kli_buffer_t` stores arbitrary bytes in a caller-owned arena.

```c
kli_buffer_t buffer;

if (kli_buffer_init_capacity(&buffer, &arena, expected_size) < 0) {
    return 1;
}

(void)kli_buffer_append(&buffer, data, size);
(void)kli_buffer_append_byte(&buffer, '\n');
```

Contracts:

- binary data is not automatically NUL-terminated;
- capacity grows geometrically from a small minimum;
- every size and pointer transition is overflow checked;
- append is overlap-safe, including self-append;
- failed growth preserves both buffer state and arena offset;
- clear resets logical length while retaining capacity;
- the buffer does not own or destroy its arena.

Growth allocates a new arena block and copies existing bytes. The old block remains
owned by the monotonic arena. Callers should reserve expected capacity when known
to avoid unnecessary arena consumption.

## Dynamic strings

`kli_string_t` adds text semantics over the binary buffer.

```c
kli_string_t text;

if (kli_string_init(&text, &arena) < 0) {
    return 1;
}

(void)kli_string_assign(&text, "ArmoniOS");
(void)kli_string_append(&text, " userland");
kli_write_cstr(ARM_FD_STDOUT, kli_string_cstr(&text));
```

A valid string maintains:

```text
buffer.length < buffer.capacity
buffer.data[buffer.length] == '\0'
```

Contracts:

- length and capacity report text bytes, excluding the terminator;
- explicit-length operations reject embedded NUL bytes;
- UTF-8 is stored as bytes without validation, normalization, or code-point
  counting;
- assign and append are overlap-safe;
- failed growth preserves text, terminator, capacity, and arena offset;
- clear returns to a valid empty string while retaining capacity;
- resetting or destroying the arena invalidates the string.

## Complete file transfers

`file.o` builds higher-level operations from the existing structured VFS calls.

```c
kli_buffer_t bytes;
kli_string_t text;

if (kli_file_read_all("/fat/data.bin", &arena, &bytes) < 0) {
    return 1;
}

if (kli_file_read_text("/fat/config.ini", &arena, &text) < 0) {
    return 1;
}

(void)kli_fd_write_all(ARM_FD_STDOUT, bytes.data, bytes.length);
```

`kli_fd_write_all` handles partial writes until the requested byte count is
complete. Zero progress is rejected, negative statuses are propagated, and an
overreported count is treated as a protocol error.

The path readers:

- query `SYS_STAT_V2` and require a regular file;
- reserve from the observed size but continue until EOF if the file grows;
- close descriptors after successful and failed transfers;
- use a fixed 256-byte stack transfer block;
- clear the destination and restore the exact entry arena offset on any failure;
- preserve arbitrary bytes for binary reads;
- reject embedded NUL bytes and guarantee termination for text reads.

There is intentionally no path-level replace/write helper yet. Without truncate or
an atomic replacement contract, writing a shorter payload over an existing longer
file could retain stale trailing bytes.

## Ownership and failure rules

All higher-level objects are caller-owned. A successful function returns a valid
object tied to the supplied arena. A failing function must leave documented state:
usually the previous object unchanged, or a cleared destination with the arena
offset restored.

Runtime modules must not:

- introduce hidden mutable globals;
- depend on libc or POSIX;
- assume allocation failure cannot occur;
- expose partially committed destination state;
- bypass typed public ABI records;
- silently consume unbounded userland stack.

## External SDK shape

A future SDK can distribute matching public headers, `crt0.o`, `libkarm.a`, the
KLI1 linker script, and image-boundary objects. The intended link model is a
normal static-library dependency rather than per-application knowledge of
internal runtime object files.

## Current boundary

`libkarm` is not a complete libc. It currently provides a practical freestanding
foundation: syscall access, minimal I/O, basic memory/string utilities, monotonic
arenas, growable byte buffers, dynamic strings, complete descriptor writes, and
rollback-safe file reads.

Remaining runtime work includes:

- formatting and parsing helpers with explicit bounds;
- line-oriented input;
- reusable path and argument helpers;
- safe truncate/replace helpers after the VFS contract exists;
- a deliberately chosen general heap only if real consumers require individual
  deallocation;
- adoption by more applications so shared code replaces duplication instead of
  merely adding library surface.
