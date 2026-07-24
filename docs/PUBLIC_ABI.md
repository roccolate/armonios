# Public ABI boundary

ArmoniOS applications must be able to compile without kernel-private headers.
Every value or layout that crosses the syscall boundary belongs under:

```text
include/armonios/abi/
```

The kernel, `libkarm`, `libarmdesk`, and applications consume the same public
definitions. Driver state, scheduler internals, VFS implementation structures,
compositor storage, and kernel ownership metadata remain private.

## Dependency direction

```text
application -> libarmdesk -> libkarm -> public ABI -> kernel
console app -------------> libkarm -> public ABI -> kernel
```

Rules:

1. Applications and userland libraries do not include kernel-private headers for
   syscall numbers, statuses, flags, or records.
2. `libkarm` remains GUI-independent.
3. `libarmdesk` may depend on `libkarm` and public ABI headers.
4. Public headers do not include kernel implementation headers.
5. Compatibility aliases may preserve old names temporarily, but they are never
   a second source of truth.

## Current public headers

| Header | Public responsibility |
|---|---|
| `base.h` | fixed-width public scalar types |
| `version.h` | compile-time ABI revision |
| `syscall_numbers.h` | frozen syscall numbers and range guards |
| `errors.h` | frozen negative status values |
| `memory.h` | mapping protection and reserved mapping flags |
| `vfs.h` | descriptors, open/seek flags, metadata, directory entries, and filesystem information |
| `process.h` | process states, exit codes, and process-list entries |
| `system.h` | memory and time information records |
| `gui.h` | GUI events, state flags, cursor values, and input button bits |

The exact syscall surface is documented in `SYSCALLS.md`.

## ABI revision

The current public ABI revision is `1.0` during pre-release development.

This revision is a compatibility label, not a runtime capability query. A kernel
or filesystem may omit an optional operation even when an application was built
against the same ABI revision. Capability-bearing interfaces such as
`SYS_FSINFO` must be used instead of inferring support from the version number.

Compatibility rules:

- syscall numbers are never renumbered or reused;
- status values are never changed or reused;
- existing structure sizes, field order, event values, and flag values remain
  fixed;
- richer incompatible semantics use a new syscall or explicitly versioned
  record;
- append-only additions may remain under the pre-release 1.0 identifier until the
  first release establishes the external compatibility baseline;
- a future major revision requires an explicit migration and compatibility plan.

## Published record layouts

Legacy records remain frozen:

```text
arm_stat_t           8 bytes
arm_meminfo_t       16 bytes
arm_timeinfo_t      24 bytes
arm_process_entry_t 24 bytes per entry
```

Versioned VFS records provide expandable contracts without changing the legacy
payloads:

```text
arm_stat_v2_t   32 bytes
arm_dirent_v2_t 96 bytes
arm_fsinfo_t    64 bytes
```

The caller initializes the version and structure-size fields. The kernel validates
the complete destination, builds a kernel-owned result, and copies it only after
provider validation succeeds.

Current versioned calls:

- `SYS_STAT_V2 = 49`;
- `SYS_READDIR_V2 = 50`;
- `SYS_FSINFO = 51`.

`SYS_FSINFO` reports runtime filesystem capabilities such as read-only state,
directory support, long-name support, truncate support, flush support, and whether
free-byte accounting is valid. Providers must not advertise a capability merely
because the generic VFS has a placeholder for it.

## Public statuses

The public error values are ArmoniOS-native statuses, not POSIX/Linux errno
compatibility. Kernel `ERR_*` names and userland `KLI_*` names are compatibility
aliases to the public values.

Current filesystem-specific additions include:

- `EXIST`;
- `NOTDIR`;
- `ISDIR`;
- `NOTEMPTY`;
- `NOSPC`;
- `ROFS`;
- `NOTSUP`;
- `RANGE`.

Existing legacy operations may preserve broader historical failure results. New
interfaces should return the most specific status the implementation can prove.

## Userland wrapper policy

`libkarm` exposes typed wrappers for public records and keeps compatibility
wrappers only where existing in-tree source still needs them.

Examples include:

```c
kli_stat_v1(path, arm_stat_t *);
kli_stat_v2(path, arm_stat_v2_t *);
kli_readdir_v2(path, start, arm_dirent_v2_t *, count);
kli_fsinfo(path, arm_fsinfo_t *);
kli_meminfo_v1(arm_meminfo_t *);
kli_timeinfo_v1(arm_timeinfo_t *);
kli_proclist_v1(arm_process_entry_t *, count);
```

New application code should prefer typed public records rather than private arrays
or duplicated layouts.

## Authority is separate from layout

A stable record layout does not make every operation safe for untrusted external
applications.

The current desktop includes cross-process operations used by the trusted panel,
including window discovery, focus, restore, and state inspection. Process kill is
also broadly available to the compiled-in environment. Before untrusted external
applications are supported, ArmoniOS needs an explicit authority or capability
model for desktop and process-control operations.

## Required change discipline

A public ABI change must update in one coherent cut:

- public headers;
- kernel dispatch and implementation;
- userland wrappers;
- compile-time layout assertions;
- focused ABI and compatibility tests;
- QEMU coverage when runtime behavior changes;
- `SYSCALLS.md`;
- affected architecture, current-state, risk, and roadmap text.

Rebuilding current applications is not sufficient proof of compatibility.
Previously built fixtures must continue to execute, or fail through an explicit,
documented compatibility boundary.

## Remaining ABI work

- add a runtime ABI/capability query for kernel-wide optional facilities;
- move remaining user-visible limits into queried capabilities where appropriate;
- add an old-SDK binary fixture gate after external KLI1 loading exists;
- define authority for cross-process desktop and process-control operations;
- version any future diagnostic ABI instead of exposing kernel-internal snapshots.
