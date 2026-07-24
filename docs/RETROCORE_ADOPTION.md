# Retrocore and rkc adoption

ArmoniOS may reuse proven concepts from `retrocore-spec` and `rkc`, but it remains
an independent bare-metal operating system.

Neither external project is a runtime, submodule, package, or build dependency of
ArmoniOS.

```text
retrocore-spec        vocabulary, contracts, and fixtures
rkc                   portable reference implementations
        |
        | selective review and local reimplementation
        v
libarmdesk             ArmoniOS desktop-facing userland layer
libkarm                ArmoniOS freestanding base runtime
public ABI             ArmoniOS kernel/userland contract
kernel and drivers     ArmoniOS-native implementation
```

## Adoption rules

1. ArmoniOS owns its syscall ABI, executable format, scheduler, process model,
   compositor, drivers, VFS, and filesystems.
2. Do not add `retrocore-spec` or `rkc` as a required runtime, submodule, or build
   dependency.
3. Adopt only a small concept or module with a concrete ArmoniOS consumer.
4. Prefer local implementation over copying a dependency graph into the OS.
5. Keep neutral models separate from platform calls and syscalls.
6. Keep memory caller-owned, capacities explicit, and failure behavior bounded.
7. Add focused host tests before enabling the result in a shipping application.
8. Measure application and kernel image impact.
9. Preserve public ABI, KLI1, stack, `.data`, ownership, and fail-closed contracts.
10. Document the resulting ArmoniOS behavior rather than describing the external
    project as part of the runtime.

## Current adopted foundation

The current merged desktop foundation contains:

- `include/armonios/abi/gui.h` as the shared GUI ABI contract;
- `programs/libarmdesk/gui.h` as the canonical typed desktop wrapper;
- `programs/libarmdesk/rect.h` as a small backend-neutral rectangle model;
- `programs/libarmdesk/theme.h` as semantic framebuffer color tokens;
- focused host tests for public layout, geometry, and theme values;
- `programs/libkarmdesk/gui.h` as a temporary compatibility include.

The geometry and semantic-theme direction was informed by small portable models,
but the source, naming, ABI, ownership, build, and tests are ArmoniOS-local.

`libkarm` is also an ArmoniOS-owned runtime. Its syscall wrappers, arena, buffer,
string, and file helpers are documented by `LIBKARM.md`; they should not be
represented as an imported external runtime.

## Current non-adoption

Current `main` does not contain a promoted generic widget toolkit.

A closed unmerged draft explored reusable controls for Control Panel. That code is
not part of the repository state and must not be documented as implemented.

Applications still draw much of their interface directly with text and rectangle
calls. The compatibility include remains in use by several applications while the
canonical `libarmdesk` path is adopted incrementally.

## Candidate modules

A candidate enters ArmoniOS only when the trigger is real.

| Candidate | Likely consumers | Adoption trigger |
|---|---|---|
| fixed-capacity text-edit model | Editor, textbox, launcher search | at least two consumers need shared insertion, deletion, caret, and selection semantics |
| list/selection model | Files, menus, selectors, Control | duplicated navigation and paging behavior exists |
| taskbar/application model | Panel | panel state can be separated from GUI syscalls without weakening process/window ownership |
| simple control state model | Control and future dialogs | real shared button/text-field behavior is needed by more than one application |
| app manifest parser | launcher and external applications | applications are no longer only embedded catalog entries |
| logical event adapter | widgets and replay fixtures | GUI event payload semantics are fully versioned and tested |
| canvas or draw-list model | icons, images, offscreen controls | a generic bitmap/blit ABI exists |

Long file names, general filesystem mutation, network sockets, scheduling,
compositing, and hardware drivers are not portable-library adoption tasks. They
remain ArmoniOS platform work.

## Promotion checklist

Before landing another externally inspired module:

1. Name the duplicated ArmoniOS behavior or concrete consumer.
2. Define ownership, capacity, lifetime, and failure contracts.
3. Keep syscall and device operations outside the neutral model.
4. Implement focused host tests, including invalid and full-capacity cases.
5. Integrate one real consumer without creating hidden global state.
6. Build QEMU and Raspberry Pi targets.
7. Run KLI1, stack, size, ABI, and relevant QEMU gates.
8. Measure binary changes.
9. Update `LIBARMDESK.md`, `LIBKARM.md`, `APPLICATIONS.md`, or another focused
   reference as appropriate.
10. Record the external source only as design provenance, not as a runtime
    dependency.

## Dependency direction

The intended dependency direction remains one-way:

```text
application
  -> libarmdesk
  -> libkarm
  -> public ABI
  -> kernel
```

Console applications may depend directly on `libkarm`. `libkarm` must remain
independent from GUI and desktop code. Public headers must remain independent from
kernel-private implementation headers.
