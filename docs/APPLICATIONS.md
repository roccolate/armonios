# Applications

ArmoniOS currently ships seven KLI1 applications embedded in the kernel image:

```text
panel
shell
editor
files
monitor
control
clock
```

It also has a minimal external application platform. A freestanding C program can
be compiled against the generated SDK, stored as a regular KLI1 file in FAT32,
and spawned in EL0 without embedding or relinking that application into
`kernel.bin`.

The visible built-in application catalog is defined by
`include/armonios/app_catalog.h`. Shell, Editor, Files, and Monitor are pinned on
the panel. Control Panel and Clock are available from the application menu. Panel
is boot-only and does not list itself as a launchable application. External files
are not automatically added to this catalog.

This document describes the applications and launch boundaries present on current
`main`. It does not turn compact demonstrations into claims of daily-use
completeness.

## Shared application model

Applications are freestanding C11 programs packaged as KLI1 images.

```text
application
  -> libarmdesk or compatibility GUI include
  -> libkarm
  -> public ArmoniOS ABI
  -> kernel
```

Built-in applications are linked into the kernel image and resolved under:

```text
/armonios/<name>
```

External applications use the same public ABI and static runtime, but are built
from the generated SDK and loaded from an absolute VFS path such as:

```text
/fat/HELLO.KLI
```

Shared rules:

- no libc, POSIX process environment, dynamic linker, or hosted runtime;
- syscall wrappers come from `libkarm`;
- canonical GUI wrappers come from `libarmdesk`;
- `programs/libkarmdesk/gui.h` remains a temporary compatibility include used by
  applications not yet migrated;
- large mutable state is mapped with `SYS_MMAP` where needed;
- shipping and external images preserve the KLI1 mutable-storage, relocation,
  size, and stack gates;
- all current built-in UI is rendered through text and rectangle GUI syscalls;
- there is no promoted reusable widget toolkit yet;
- external applications are currently trusted sideloaded programs, not hostile
  sandbox workloads.

The full external SDK and loader contract is documented in
[`EXTERNAL_APPLICATIONS.md`](EXTERNAL_APPLICATIONS.md).

## Panel

**Source:** `programs/apps/panel.c` and `panel_runtime.inc`

Panel is the boot-owned desktop taskbar and application launcher.

Implemented behavior:

- application menu generated from the shared built-in application catalog;
- pinned launch buttons for Shell, Editor, Files, and Monitor;
- launch of additional catalog applications from the menu;
- process-list refresh and association of running processes with application
  entries;
- focused, running, and minimized visual states;
- focus or restore of an existing application window;
- multiple-instance indicators within fixed panel capacity;
- uptime clock text;
- keyboard and mouse handling for the panel and popup menu.

Current limits:

- the application model and displayed instance counts are fixed-capacity;
- process/window discovery uses the current process-list and window-for-PID ABI;
- external KLI files do not automatically become menu entries or pinned apps;
- the panel is not a general desktop shell, notification center, tray protocol, or
  extensible plugin host;
- icons are currently compact text markers rather than a bitmap/icon system.

## Shell

**Source:** `programs/apps/shell.c`

Shell is a windowed command interface with a userland-maintained current directory,
command history, and scrollback.

Implemented commands:

```text
help
pwd
cd [path]
ls [path]
cat <path>
run <application-or-path> [arguments...]
kill last
mem
ps
ticks
exit
```

Interaction features:

- four-entry command history through Up/Down;
- sixteen-line circular output log;
- Page Up/Page Down scrollback;
- relative and absolute path construction;
- built-in launch through `/armonios/<name>` and `SYS_SPAWN_ARGV`;
- external application launch from an absolute VFS path, for example
  `run /fat/HELLO.KLI`;
- display of memory, timer, and process information.

Current limits:

- paths and command lines use small fixed buffers;
- arguments are whitespace-separated only; there is no quoting, escaping,
  redirection, pipelines, variables, scripts, globbing, or job control;
- external applications must already exist as valid KLI1 regular files;
- `run` does not discover packages, add extensions, or install dependencies;
- `kill last` remembers only the last PID launched by that Shell instance;
- `cat` reads at most eight 128-byte chunks and reports truncation after that;
- directory listing still uses the legacy byte-stream wrapper rather than the
  structured paginated interface;
- the current directory is application state, not a kernel process property.

## Editor

**Source:** `programs/apps/editor.c`

Editor is a compact text editor for one file. It opens `argv[1]` when supplied and
otherwise uses `/tmp/note`.

Implemented behavior:

- file load into a fixed 512-byte buffer;
- printable insertion and backspace;
- newline insertion;
- Left, Right, Up, and Down caret movement;
- visible caret block;
- Ctrl-S save;
- Ctrl-Q and title-bar close;
- file launch from Files through `spawn_argv`.

Current limits:

- maximum content is 511 bytes plus the terminator;
- the view renders only the line containing the caret, capped by the render buffer;
- there is no selection, clipboard, undo, search, syntax highlighting, or scrollable
  multi-line viewport;
- save seeks to offset zero and writes the current buffer, but the VFS has no
  truncate operation yet;
- replacing an existing file with shorter content may therefore leave stale bytes
  at the old tail;
- safe replace-file semantics require truncate or an atomic replacement workflow.

## Files

**Source:** `programs/apps/files.c`

Files is a compact manager for the `/fat` root directory.

Implemented behavior:

- filesystem identity, capacity, and read-only/read-write display through
  `SYS_FSINFO`;
- structured directory records through `SYS_READDIR_V2`, with a legacy fallback;
- file type and size metadata;
- keyboard selection;
- open regular files in Editor;
- create a root 8.3 file;
- rename a root 8.3 file;
- delete a root regular file;
- explicit deletion confirmation;
- manual refresh.

Current limits:

- the view requests and displays at most eight entries;
- it does not paginate beyond the first eight entries;
- it always lists `/fat` and has no current-directory model;
- directories are displayed but cannot be entered, renamed, or deleted;
- nested read traversal exists in the VFS, but Files does not expose it yet;
- create, rename, and delete remain root-only 8.3 operations;
- Files does not currently execute a selected `.KLI` file; use Shell `run`;
- there are no long names, copy, move, properties dialog, free-space accounting,
  mounts view, or recycle bin.

## Monitor

**Source:** `programs/apps/monitor.c`

Monitor is a read-only system-information window.

Implemented behavior:

- free physical page count;
- timer tick count;
- up to six process entries showing PID, state, and name;
- periodic refresh;
- close through `q`, `Q`, or the title bar.

Current limits:

- no CPU percentage, per-process memory, runtime-service telemetry, network
  statistics, process inspection, sorting, scrolling, or process actions;
- refresh timing is yield-count based;
- runtime-service telemetry remains kernel-internal and is not exposed through a
  public ABI.

## Control Panel

**Source:** `programs/apps/control.c`

Control Panel is a small INI registry editor backed by `/fat/CONFIG.INI`.

It currently exposes fixed entries for system, display, input, files, editor, and
diagnostic preferences. The user can select an entry, edit its value, restore the
loaded values, restore built-in defaults, and save the serialized INI document.

Current limits:

- the schema is compiled into the application and is not discovered dynamically;
- values are free-form strings with no type-specific validation;
- persistence writes the serialized document from offset zero without truncate;
- saving a shorter configuration over a longer existing file may leave stale
  trailing bytes;
- the presence of a persisted preference does not by itself prove that another
  subsystem currently consumes or applies it;
- there are no category pages, real reusable controls, live preview, validation
  dialogs, or settings service;
- the closed unmerged generic-widget work is not part of this application or
  current `main`.

## Clock

**Source:** `programs/apps/clock.c`

Clock displays system uptime as `HH:MM:SS` using `SYS_TIMEINFO`.

Implemented behavior:

- mapped runtime state;
- periodic redraw;
- clean close through `q`, `Q`, or title-bar close.

Current limits:

- this is uptime, not wall-clock or calendar time;
- the format assumes a 100 Hz timer;
- hours use two decimal positions and are not designed as a long-duration time
  format;
- refresh timing is yield-count based rather than a blocking timer wait.

## External console example

**Source template:** `sdk/examples/hello-console/`

The generated SDK includes a console example that writes through the public
standard-output syscall and exits normally. It is intentionally small enough for
the current 8192-byte KLI1 process image slot.

Implemented workflow:

```sh
make sdk
make external-kli-image
make qemu-external-kli
```

Then in Shell:

```text
run /fat/HELLO.KLI
```

Permanent tests also build a dedicated external parent KLI that spawns a second
copy from the same FAT32 path, waits for the child, and exits cleanly. That source
exists for runtime verification; it is not another built-in application.

## Application catalog and build boundaries

The built-in catalog defines visible runtime metadata:

- application identifier;
- menu label;
- `/armonios/` path;
- pinned-panel flag;
- clock-widget flag.

The `Makefile` separately lists built-in application objects and linker sections.
Keep these roles distinct: the catalog is runtime metadata; build lists and linker
markers define embedded image construction.

An external application does not need a catalog entry or kernel linker section.
It needs:

1. public SDK headers and archives;
2. a valid generic KLI1 link;
3. relocation, `.data`/`.bss`, stack, and image-size gates;
4. placement in a mounted filesystem;
5. an explicit absolute path supplied to `run` or `kli_spawn`.

When adding a built-in application:

1. add its KLI1 source and image markers;
2. update the explicit build/link lists;
3. add catalog metadata when it should be visible;
4. use public ABI headers and `libkarm`/`libarmdesk` rather than kernel-private
   constants;
5. add stack and KLI1 contract coverage;
6. add focused host or QEMU tests for its behavior;
7. update this document and current-state documentation when product capability
   changes.

When publishing an external example, do not add it to the embedded app list merely
to simplify testing. Build it through the SDK and exercise the normal FAT32/VFS
loader path.

## Roadmap boundary

The next application work should follow the platform order rather than duplicate
missing infrastructure inside each program:

1. promote compiled `libarmdesk` application and window objects;
2. add the desktop library to the external SDK;
3. demonstrate a windowed external KLI with a label and button;
4. migrate compatibility includes to the canonical desktop layer;
5. then expand Files, Editor, Shell, Monitor, Control, Panel, and Clock into more
   complete tools.
