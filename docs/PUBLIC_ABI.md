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

- `base.h` — fixed-width public scalar types such as `arm_status_t`, `arm_pid_t`,
  and `arm_fd_t`;
- `version.h` — compile-time ABI major/minor revision;
- `syscall_numbers.h` — frozen syscall numbers and range guards;
- `errors.h` — frozen negative syscall status values;
- `memory.h` — `SYS_MMAP` protection bits and reserved mapping flags;
- `vfs.h` — standard descriptors, open/seek flags, and the current `SYS_STAT`
  payload;
- `process.h` — observable process states, kernel-generated exit codes, and the
  `SYS_PROCLIST` entry layout;
- `system.h` — current `SYS_MEMINFO` and `SYS_TIMEINFO` payload layouts;
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
5. Compatibility headers and aliases may temporarily preserve old include paths
   and names, but the public header remains the source of truth.

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

The current information calls keep their existing byte layouts:

```text
SYS_STAT      arm_stat_t          8 bytes
SYS_MEMINFO   arm_meminfo_t      16 bytes
SYS_TIMEINFO  arm_timeinfo_t     24 bytes
SYS_PROCLIST  arm_process_entry_t 24 bytes per entry
```

These names formalize the existing ABI; they do not change runtime behavior.
Future richer metadata must use new, versioned calls or structures rather than
silently growing these payloads.

## libkarm compatibility

`libkarm` now offers typed wrappers for the public payloads:

```c
kli_stat_v1(path, arm_stat_t *);
kli_meminfo_v1(arm_meminfo_t *);
kli_timeinfo_v1(arm_timeinfo_t *);
kli_proclist_v1(arm_process_entry_t *, count);
```

The historical untyped or array-based wrappers remain available so existing
source continues to compile. New applications should prefer the typed forms.

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

1. Introduce size/version-prefixed structures only for new expandable syscalls.
2. Add an ABI/capability query syscall without inferring features from version.
3. Move remaining user-visible limits into capability reporting instead of
   freezing kernel implementation limits as compile-time constants.
4. Add an old-SDK binary fixture gate once external KLI loading exists.
5. Introduce explicit authority for cross-process desktop and process-control
   operations before untrusted third-party applications are supported.
