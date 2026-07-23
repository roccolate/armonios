# libarmdesk

`libarmdesk` is the ArmoniOS desktop-facing userland layer.

It may depend on `libkarm`; `libkarm` must not depend on it.

Current foundation:

- `gui.h` — typed GUI syscall wrappers;
- `rect.h` — backend-neutral rectangle and clipping helpers;
- `theme.h` — semantic framebuffer color tokens.

`programs/libkarmdesk/gui.h` is a compatibility include for existing apps and
should be removed only after all applications use the new path.

Future widgets, layouts, dialogs, notifications, and icon support belong here.
The tentative ArmoniIcons direction is recorded in
[`docs/ARMONIICONS_PROPOSAL.md`](../../docs/ARMONIICONS_PROPOSAL.md); it is not
an approved ABI or implementation plan.

Kernel policy, compositor internals, and drivers do not belong in `libarmdesk`.
