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
```

Applications link against the archive instead of selecting those objects by
hand. GNU `ld` extracts only archive members needed by the application, and the
existing `-ffunction-sections`, `-fdata-sections`, and `--gc-sections` settings
remove unused sections from extracted members.

This replaces the old per-application `APP_LIBS_*` lists without forcing every
application to carry all string or I/O helpers.

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
- A change to startup calling convention, argument construction, or process
  exit behavior is an ABI-sensitive change and requires dedicated tests and
  documentation.

## Current limitations

`libkarm` is not yet a complete libc. It currently provides syscall wrappers,
minimal output, memory/string helpers, and integer conversion. Heap allocation,
dynamic strings, buffers, formatted output, and higher-level file helpers are
future runtime cuts built on top of this archive foundation.
