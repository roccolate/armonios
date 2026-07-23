# libarmdesk

`libarmdesk` is the ArmoniOS desktop-facing userland layer.

It may depend on `libkarm`; `libkarm` must not depend on it.

Current foundation:

- `gui.h` — typed GUI syscall wrappers;
- `rect.h` — backend-neutral rectangle and clipping helpers;
- `theme.h` — semantic framebuffer color tokens;
- `layout.h` — bounded caller-owned row and column placement;
- `event.h` — absolute desktop pointer to content-local conversion;
- `widget.h` — caller-owned label and button models;
- `render.h` — label and button adapters over the existing draw syscalls.

Control is the first shipping consumer. It uses the shared theme, geometry,
layout, label, button, renderer, pointer conversion, and cursor-region contracts
while retaining its application-owned registry list and persistence logic.

The neutral models are freestanding, fixed-capacity, host-testable, and contain
no syscall dependency. Platform calls stay in `gui.h` and `render.h`.

`programs/libkarmdesk/gui.h` remains a compatibility include for applications
that have not yet migrated to the canonical path. Remove it only after all
shipping applications use `libarmdesk` directly.

Future list views, text models, dialogs, notifications, and ArmoniIcons belong
here. Kernel policy, compositor internals, drivers, filesystem code, and
application-specific state do not.
