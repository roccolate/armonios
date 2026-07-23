# Public ABI Boundary

## Purpose

ArmoniOS applications must be able to compile independently from kernel-private
headers. The public boundary lives under:

```text
include/armonios/abi/
```

Kernel and userland consume the same public definitions for every value or
layout that crosses the syscall boundary. Kernel implementation structures,
driver state, scheduler internals, compositor storage, and VFS internals remain
private.

## Current public headers

- `version.h` — compile-time ABI major/minor revision;
- `syscall_numbers.h` — frozen syscall numbers and range guards;
- `errors.h` — frozen negative syscall status values;
- `gui.h` — GUI event layout and public GUI constants.

## Dependency direction

```text
applications -> libarmdesk -> libkarm -> public ABI -> kernel
```

Rules:

1. Applications, `libkarm`, and `libarmdesk` must not include kernel-private
   headers to obtain syscall numbers, error values, flags, or ABI structures.
2. `libkarm` must remain GUI-independent.
3. `libarmdesk` may depend on `libkarm` and public ABI headers.
4. The kernel may consume public ABI headers, but public ABI headers must not
   include kernel implementation headers.
5. Compatibility headers may temporarily preserve old include paths, but the
   public header remains the source of truth.

## Compatibility rules

The current public ABI revision is `1.0`.

- Existing syscall numbers are never renumbered or reused.
- Existing error values are never changed or reused.
- Existing public structure sizes, field order, event IDs, and flag values are
  frozen unless an explicitly versioned replacement is introduced.
- Minor revisions are append-only.
- A major revision is reserved for an intentional incompatible boundary and
  requires an explicit compatibility or migration plan.
- Compile-time ABI versioning does not imply runtime feature availability. A
  future query/capability syscall must report optional facilities explicitly.

## Required change discipline

Any public ABI change must update in one change set:

- public headers;
- kernel implementation or dispatch;
- `libkarm` / `libarmdesk` wrappers;
- host ABI tests;
- QEMU coverage where behavior changes;
- `docs/SYSCALLS.md` and current-state documentation.

A change is not complete merely because the current applications rebuild. ABI
compatibility means previously built fixtures continue to execute or fail with a
documented compatibility error instead of silently changing behavior.

## Next cuts

1. Move public VFS flags and structured metadata into public ABI headers.
2. Move process exit codes and public process-list layouts.
3. Introduce size/version-prefixed structures for new expandable syscalls.
4. Add an ABI/capability query syscall without inferring features from version.
5. Add an old-SDK binary fixture gate once external KLI loading exists.
6. Introduce explicit authority for cross-process desktop and process-control
   operations before untrusted third-party applications are supported.
