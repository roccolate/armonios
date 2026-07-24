# libkarm

## Purpose

`libkarm` is the freestanding base runtime for ArmoniOS user applications. It
sits directly above the public syscall ABI and remains independent from the
desktop toolkit:

```text
application -> libarmdesk -> libkarm -> public ABI -> kernel
console app -------------> libkarm -> public ABI -> kernel
```

`libarmdesk` may depend on `libkarm`. `libkarm` must not depend on GUI or
desktop code.

## Build artifacts

The build produces two different artifacts:

```text
build/programs/libkarm/crt0.o
build/programs/libkarm/libkarm.a
```

They intentionally have different roles.

### `crt0.o`

`crt0.o` owns the user-image entry point. It prepares `argc` and `argv`, calls
`main`, and terminates the process through `SYS_EXIT`.

It remains an explicit startup object rather than an archive member so `_start`
is always present and the image link order stays obvious.

### `libkarm.a`

The static archive contains reusable runtime members:

```text
syscall.o
io.o
string.o
arena.o
arena_map.o
buffer.o
```

Applications link against the archive instead of selecting those objects by
hand. GNU `ld` extracts only archive members needed by the application, and the
existing `-ffunction-sections`, `-fdata-sections`, and `--gc-sections` settings
remove unused sections from extracted members.

This replaces the old per-application `APP_LIBS_*` lists without forcing every
application to carry all runtime helpers.

## Arena allocator

The first dynamic-memory primitive is an explicit monotonic arena:

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

The design is intentionally small:

- no global allocator state;
- one `SYS_MMAP` region can serve many small allocations;
- allocations are overflow checked;
- custom alignment must be a power of two;
- failed allocations do not consume capacity;
- `reset` releases all allocations logically in constant time;
- `destroy` uses the same requested mapping size with `SYS_MUNMAP`;
- failed unmap preserves the arena state so callers can retry or diagnose it.

`kli_arena_init` can also place an arena over caller-provided storage. The pure
allocation core therefore has focused host tests independent from the syscall
layer. `arena_map.c` is tested with syscall stubs for mmap/unmap lifecycle and
error propagation.

An arena is not a general-purpose heap. Individual objects cannot be freed and
a caller must destroy an existing mapped arena before mapping another region
into the same `kli_arena_t`.

## Dynamic binary buffer

`kli_buffer_t` stores arbitrary bytes inside a caller-owned arena:

```c
kli_buffer_t buffer;

if (kli_buffer_init(&buffer, &arena) < 0) {
    return 1;
}

(void)kli_buffer_append(&buffer, header, header_size);
(void)kli_buffer_append_byte(&buffer, '\n');
```

The buffer starts empty and reserves at least 64 bytes on first growth. Later
growth doubles capacity until it can satisfy the requested append. All length,
capacity, pointer, and addition arithmetic is overflow checked.

Growth allocates a new arena block and copies the existing bytes. The previous
block remains owned by the arena. This keeps the implementation deterministic
and avoids pretending that individual arena allocations can be freed, but it
also means repeated small growth wastes arena capacity. Callers that know an
expected size should use `kli_buffer_init_capacity` or `kli_buffer_reserve`.

Additional rules:

- the buffer never owns or destroys its arena;
- failed reserve/append leaves buffer and arena offsets unchanged;
- append uses overlap-safe copying, including self-append;
- `clear` resets logical length without releasing capacity;
- zero-byte append succeeds even when its source pointer is null;
- this is a binary buffer, not a null-terminated string.

## Dynamic text string

`kli_string_t` layers null-terminated text semantics over `kli_buffer_t`:

```c
kli_string_t text;

if (kli_string_init(&text, &arena) < 0) {
    return 1;
}

(void)kli_string_assign(&text, "ArmoniOS");
(void)kli_string_append(&text, " BASIC");
(void)kli_string_append_char(&text, '!');

kli_write_cstr(ARM_FD_STDOUT, kli_string_cstr(&text));
```

A valid string always satisfies:

```text
buffer.length < buffer.capacity
buffer.data[buffer.length] == '\0'
```

`kli_string_length` reports text bytes and excludes the terminator.
`kli_string_capacity` reports usable text capacity and also excludes the
terminator. Empty strings still own valid storage and return a usable `""`
through `kli_string_cstr`.

Text rules:

- C-string forms use `strlen` and never include the source terminator;
- `_n` forms accept explicit byte lengths but reject embedded NUL bytes;
- UTF-8 can be stored as bytes, but libkarm does not validate, normalize, or
  count Unicode code points;
- `append_char` rejects `\0` so logical length and C-string length cannot drift;
- assign and append use overlap-safe copying, including self-append that grows
  into a new arena block;
- failed growth preserves the old text, terminator, capacity, and arena offset;
- `clear` restores the empty string while retaining allocated capacity;
- resetting or destroying the backing arena invalidates every string using it.

The dynamic string implementation shares `buffer.o` with the binary-buffer
layer. Function sections and `--gc-sections` keep unused string operations out
of applications that only need raw buffers.

## In-tree application link

The effective link order is:

```text
application.o
application_header.o
crt0.o
libkarm.a
application_end.o
```

The KLI header and end markers remain explicit build boundaries. `libkarm.a`
does not contain either marker and does not own the executable format.

## External SDK link

A future standalone SDK can expose the same shape:

```sh
aarch64-linux-gnu-ld \
    --gc-sections \
    -T image.ld \
    app.o app_header.o crt0.o \
    -L "$ARMONIOS_SDK/lib" -lkarm \
    app_end.o \
    -o app.elf
```

The SDK must distribute matching public ABI headers, `crt0.o`, `libkarm.a`, and
the supported application linker script.

## Compatibility rules

- `crt0.o` is startup code, not a general runtime library member.
- `libkarm.a` remains GUI-independent.
- Public syscall values and layouts come from `include/armonios/abi/`.
- Adding a runtime module requires adding its object to
  `LIBKARM_ARCHIVE_OBJS`; applications must not regain private per-app object
  lists.
- Runtime modules should use function/data sections so unused code remains
  removable.
- Runtime objects must not introduce hidden mutable globals while KLI1 forbids
  mutable `.data` and `.bss` in shipping applications.
- A change to startup calling convention, argument construction, or process
  exit behavior is an ABI-sensitive change and requires dedicated tests and
  documentation.

## Current limitations

`libkarm` is not yet a complete libc. It currently provides syscall wrappers,
minimal output, memory/string helpers, integer conversion, monotonic arenas,
growable binary buffers, and arena-backed dynamic strings. A reusable free-list
heap, formatted output, and higher-level file helpers are future runtime cuts
built on this foundation.
