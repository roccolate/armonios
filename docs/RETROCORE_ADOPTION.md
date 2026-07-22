# Retrocore and rkc adoption

ArmoniOS uses `retrocore-spec` as shared vocabulary and may reuse proven ideas
from `rkc`, but it remains a self-contained bare-metal product.

```text
retrocore-spec  contracts and fixtures
        |
        v
rkc             portable reference implementations
        |
        v
libarmdesk      ArmoniOS-local desktop API and adapters
        |
        v
libkarm         userland runtime and syscall access
        |
        v
kernel
```

## Rules

- Do not add `retrocore-spec` or `rkc` as runtime, submodule, or build
  dependencies.
- Keep the syscall ABI, compositor, drivers, scheduler, loader, and filesystem
  implementation ArmoniOS-native.
- Reimplement or selectively import only small modules that remove duplicated
  product code.
- Keep each adopted module freestanding, caller-owned, bounded, and testable on
  the host.
- Measure application and kernel image deltas before enabling new code in
  shipping apps.

## Foundation adopted

The first adoption slice adds no new shipping application behavior:

- `include/armonios/abi/gui.h` is the shared kernel/userland GUI ABI contract.
- `programs/libarmdesk/gui.h` is the canonical desktop syscall wrapper.
- `programs/libkarmdesk/gui.h` remains as a temporary compatibility include.
- `programs/libarmdesk/rect.h` provides ArmoniOS-local geometry helpers based on
  the small rectangle model proven in `rkc`.
- `programs/libarmdesk/theme.h` exposes semantic color tokens aligned with
  `retrocore-spec/contracts/theme-tokens.md`.
- A standalone host test protects ABI size, token values, and rectangle
  behavior.

## Deferred candidates

Adopt these only with a concrete ArmoniOS consumer and a measured size budget:

| Candidate | Intended consumer | Trigger |
|---|---|---|
| fixed text buffer model | Editor, textbox, launcher search | shared text-input widget work |
| list model | Files, menus, selectors | two apps need identical selection behavior |
| taskbar model | Panel | panel is moved onto libarmdesk models |
| app manifest parser | launcher and external apps | apps are no longer only compiled-in blobs |
| canvas/draw list | icons, offscreen widgets, images | generic bitmap/blit path exists |
| logical event adapter | widgets and replay fixtures | `data1`/`data2` meanings are fully documented |

## Promotion checklist

Before another `rkc`-inspired module enters ArmoniOS:

1. Identify the duplicated local implementation it replaces.
2. Keep platform calls outside the neutral model.
3. Add host tests for edge cases and fixed-capacity behavior.
4. Build QEMU and RPi4 targets.
5. Run the kernel size and per-app stack gates.
6. Update this document and the matching `retrocore-spec` ArmoniOS adapter.
