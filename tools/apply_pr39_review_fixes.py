#!/usr/bin/env python3

from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    target = Path(path)
    text = target.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    target.write_text(text.replace(old, new, 1))


replace_once(
    "kernel/syscall_gui.c",
    '''    gui_desktop_t *desktop = gui_desktop();
    gui_window_t *window;
    uint32_t state = 0;
    int64_t status;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS) {
''',
    '''    gui_desktop_t *desktop = gui_desktop();
    gui_window_t *window;
    uint32_t state = 0;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS) {
''',
)

replace_once(
    "docs/TECHNICAL_RISKS.md",
    '''| RISK-012 | P1 for v0.2 | Syscall buffer ownership | CLOSED | VFS, argv, IPC, GUI, and information syscall payloads cross through bounded kernel-owned temporaries. |
| RISK-015 | P2 hardening | Fault-contained copy | OPEN | User-copy transfers remain ordinary EL1 loads/stores without exception recovery. |
| RISK-013 | P1 for v1 | Storage/VFS | OPEN | Current VFS/FAT path is too narrow for the v1 filesystem target. |
| RISK-014 | P1 for v1 | Desktop apps | OPEN | Current apps are useful demos, not complete daily-use tools. |
''',
    '''| RISK-012 | P1 for v0.2 | Syscall buffer ownership | CLOSED | VFS, argv, IPC, GUI, and information syscall payloads cross through bounded kernel-owned temporaries. |
| RISK-013 | P1 for v1 | Storage/VFS | OPEN | Current VFS/FAT path is too narrow for the v1 filesystem target. |
| RISK-014 | P1 for v1 | Desktop apps | OPEN | Current apps are useful demos, not complete daily-use tools. |
| RISK-015 | P2 hardening | Fault-contained copy | OPEN | User-copy transfers remain ordinary EL1 loads/stores without exception recovery. |
''',
)
replace_once(
    "docs/TECHNICAL_RISKS.md",
    '''### RISK-012 — Kernel-owned syscall buffers

VFS buffers and paths, argv, IPC messages, GUI output, and system-information output are copied through bounded kernel-owned temporaries before lower layers use them. Invalid or read-only destinations are rejected before state-consuming receives dequeue data.

**Evidence:** host user-copy boundary regressions plus the complete `tools/verify.sh` matrix recorded with the closing change.

### RISK-013 — Storage and VFS are too narrow for v1
''',
    '''### RISK-013 — Storage and VFS are too narrow for v1
''',
)
replace_once(
    "docs/TECHNICAL_RISKS.md",
    '''## Closed v0.1 risks

Closed risks remain summarized here so the current release claim can be audited without carrying old branch or PR history into the new first commit.

### RISK-001 — User-copy permissions
''',
    '''## Closed risks

Closed risks remain summarized here so the current release claim can be audited without carrying old branch or PR history into the new first commit.

### RISK-012 — Kernel-owned syscall buffers

VFS buffers and paths, argv, IPC messages, GUI output, and system-information output are copied through bounded kernel-owned temporaries before lower layers use them. Invalid or read-only destinations are rejected before state-consuming receives dequeue data.

**Evidence:** host user-copy boundary regressions plus the complete `tools/verify.sh` matrix recorded with the closing change.

### RISK-001 — User-copy permissions
''',
)

Path(__file__).unlink()
