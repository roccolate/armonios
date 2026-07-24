# libarmdesk

`libarmdesk` is the desktop-facing userland layer for ArmoniOS applications.

```text
graphical application
  -> libarmdesk
  -> libkarm
  -> public ABI
  -> kernel GUI syscalls
```

It may depend on `libkarm`. `libkarm` must not depend on it.

## Current foundation

The canonical source directory is:

```text
programs/libarmdesk/
```

The merged foundation currently provides:

- `gui.h` — typed desktop syscall wrappers;
- `rect.h` — backend-neutral rectangle and clipping helpers;
- `theme.h` — semantic framebuffer color tokens.

The public kernel/userland GUI values and `gui_event_t` layout live in:

```text
include/armonios/abi/gui.h
```

Compositor state, window pools, input-driver details, and kernel drawing internals
remain private.

## Compatibility include

```text
programs/libkarmdesk/gui.h
```

is a temporary compatibility include for existing applications. It forwards to
the canonical `libarmdesk` path and should be removed only after every application
has migrated.

New code should include `programs/libarmdesk/...` directly.

## What is not implemented

Current `main` does not contain a promoted reusable widget toolkit.

In particular, the closed unmerged Control/widget work is not part of the
repository state. Do not document generic buttons, text fields, layouts, dialogs,
notifications, or list widgets as implemented merely because a draft PR once
contained them.

Current applications still implement much of their control behavior locally.

## GUI wrapper responsibility

`libarmdesk` should provide typed, narrow wrappers over the existing GUI ABI. It
must not bypass kernel ownership checks or duplicate public syscall numbers.

Wrapper rules:

- return ArmoniOS statuses without inventing POSIX semantics;
- use public ABI structures and flags;
- validate obvious local arguments before issuing a syscall where useful;
- preserve caller ownership of application state;
- avoid hidden mutable global state;
- remain freestanding and compatible with KLI1 constraints;
- keep size and stack cost measurable per application.

## Geometry helpers

Rectangle helpers are pure caller-owned value operations. They support clipping
and intersection behavior without depending on the kernel compositor.

They should remain:

- host-testable;
- overflow-aware where arithmetic expands;
- independent from framebuffer globals;
- usable by applications without allocating memory;
- consistent with public coordinate and bounds conventions.

## Theme tokens

Theme values are semantic color tokens for the current direct framebuffer drawing
model. They provide shared vocabulary, not a dynamic theme engine or persistent
settings system.

A token change can alter every application that instantiates it. Measure binary
and visible effects and avoid treating semantic names as a stable external theme
ABI unless deliberately published.

## Ownership and authority

Most window mutation remains owner-only in the kernel. The trusted panel uses a
small set of cross-process presentation calls for discovery, focus, restore, and
state inspection.

`libarmdesk` must not casually broaden those exceptions. A future environment
that loads untrusted external applications needs an explicit authority model
before exposing desktop-control helpers as generally safe.

See `GUI_ABI_NOTES.md` and `PUBLIC_ABI.md`.

## Widget promotion rules

A reusable widget or model belongs in `libarmdesk` only when:

1. at least two real applications need the same behavior, or one foundational
   application provides a justified first consumer;
2. state ownership and failure behavior are explicit;
3. drawing policy is separated from neutral state where practical;
4. fixed-capacity or dynamic-storage requirements are documented;
5. host tests cover interaction and boundary cases;
6. one shipping application adopts it in the same cut;
7. per-application stack and image deltas are measured;
8. the compatibility include and existing local implementations are not left as
   permanent duplicate paths.

Likely future areas include buttons, text input, list selection, scrolling,
layout, dialogs, and notifications. These remain roadmap work until merged and
used.

## Retrocore relationship

ArmoniOS may reuse vocabulary or small proven models from `retrocore-spec` and
`rkc`, but those projects are not runtime, submodule, or build dependencies.

Adopted ideas must be revalidated as ArmoniOS-local, freestanding, bounded code.
See `RETROCORE_ADOPTION.md`.

## Change checklist

A `libarmdesk` change should consider:

- public GUI ABI values and layouts;
- `SYSCALLS.md` and `GUI_ABI_NOTES.md`;
- owner-only and cross-process authority rules;
- compatibility includes;
- focused host tests;
- affected application migration;
- KLI1 `.data`/`.bss` restrictions;
- per-application stack and image size;
- deterministic QEMU focus/event evidence;
- visible validation when layout or interaction changes.
