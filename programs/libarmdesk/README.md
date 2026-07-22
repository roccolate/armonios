# libarmdesk

`libarmdesk` is the ArmoniOS desktop-facing userland layer.

It may depend on `libkarm`; `libkarm` must not depend on it.

Current foundation:

- `gui.h` — typed GUI syscall wrappers;
- `rect.h` — backend-neutral rectangle and clipping helpers;
- `theme.h` — semantic framebuffer color tokens.

`programs/libkarmdesk/gui.h` is a compatibility include for existing apps and
should be removed only after all applications use the new path.

Future widgets, layouts, dialogs, notifications, and ArmoniIcons belong here.
Kernel policy, compositor internals, and drivers do not.
