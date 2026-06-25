# Kernel technical-debt review

Scope: `kernel/`, `drivers/`, and the panel/apps support code we just
had to debug. The goal is to surface complexity that is going to bite
the next person who touches the code, not to propose a rewrite. Each
section names a file, an excerpt of the smell, and a concrete
direction. Order is roughly "biggest payoff first".

---

## 1. `kernel/gui.c` is 1857 lines doing five jobs

```
$ wc -l kernel/gui.c
1857 kernel/gui.c
```

gui.c mixes:

- a fixed pool of `gui_window_t` slots and their lifecycle
  (create / destroy / move / resize / focus / minimise / restore),
- a per-window backing-buffer allocator with capacity tracking,
- an event queue per window (push / pop / overflow),
- damage-rectangle tracking for the partial-repaint compositor,
- input dispatch (`gui_dispatch_input`, drag state, button masks,
  cursor regions),
- the actual framebuffer blit (`gui_draw`, `gui_render`),
- the global singleton accessor (`gui_desktop`, `g_gui_desktop`).

Symptom: every time I add a window feature, I have to find the right
slot in this file. The recent cursor-region work (commit b12761b)
touched `gui_create_window_for_pid`, `gui_destroy_window`,
`gui_refresh_cursor_shape`, plus the header, plus every test that
constructs a window by hand. The coupling is not yet catastrophic
but the file is past the "fits in one sitting" line the AGENTS.md
guideline calls out.

Direction: split into:
- `kernel/gui_pool.{c,h}` — window struct, create / destroy / move /
  resize / focus / minimise / restore / bounds.
- `kernel/gui_backing.{c,h}` — per-window backing buffer + capacity.
- `kernel/gui_input.{c,h}` — drag state, cursor shape, button masks,
  cursor regions.
- `kernel/gui_compositor.{c,h}` — damage rects, draw, render, singleton.
- `kernel/gui_events.{c,h}` — per-window event queue.

The single shared struct (`gui_window_t`, `gui_desktop_t`) can stay
in `gui.h` so the rest of the kernel still includes one header.

---

## 2. Syscall dispatch is a 1000-line switch with no central
   validation / ownership table

```
$ grep -c "^static int64_t sys_\|case SYS_" kernel/syscall.c
```

37 `sys_*` functions, ~30 dispatch cases, each one hand-rolling
ownership / pointer / length / output-buffer validation. Looking at
the entry for `sys_window_set_title`:

```c
if (process == 0 || window_id > UINT32_MAX || title_ptr == 0 ||
    title_ptr > UINT32_MAX || title_h > UINT32_MAX) {
    return ERR_INVAL;
}
const gui_window_t *window = gui_window_lookup(gui_desktop(),
                                               (uint32_t)window_id);
if (window == 0 || window->used == 0U) {
    return ERR_NOENT;
}
if (window->owner_pid != GUI_NO_OWNER &&
    window->owner_pid != (uint32_t)process->pid) {
    return ERR_PERM;
}
```

That same pattern repeats for draw_text / draw_rect / set_bounds /
minimize / restore / state / cursor_register_region. Each handler
also has its own copy of the "read N bytes from user memory" or
"write N bytes to user memory" glue.

Direction: a small `kernel/syscall_helpers.{c,h}` with:

- `sys_user_str_in(ptr, len)` — validate + copy a string.
- `sys_user_buf_in(ptr, len)` / `sys_user_buf_out(ptr, len)` — same
  for raw buffers.
- `sys_owner_window(process, window_id)` — `gui_window_lookup` +
  ownership check, returning `ERR_NOENT` or `ERR_PERM` as
  appropriate.
- A dispatch macro `DISPATCH_OWNER_WINDOW(name, ...)`.

This collapses roughly 30 lines per syscall into 5.

---

## 3. Two `gui_window_*` naming conventions live side by side

`gui.h` exports `gui_window_create`, `gui_window_set_title`,
`gui_window_set_bounds`, `gui_window_state`, `gui_window_draw_text`,
`gui_window_draw_rect`, `gui_window_flush` (syscalls), AND
`gui_set_window_title`, `gui_set_window_title_bar`,
`gui_set_window_flags` (kernel helpers), AND `gui_window_push_event`,
`gui_window_pop_event` (event queue), AND `gui_window_get_bounds`,
`gui_window_minimize`, `gui_window_restore`, `gui_window_ensure_backing`,
`gui_window_free_backing` (state).

Looking for "the helper that sets the title" can mean either
`gui_set_window_title` (kernel-only) or `gui_window_set_title`
(syscalls). They differ by ownership validation but the names give
no hint. Same problem with `gui_set_window_title_bar` vs
`gui_window_set_title`.

Direction: pick one convention and apply it uniformly.
`gui_window_*` reads better for "I am the syscall-facing API";
`gui_window_*_internal` or moving the kernel-only helpers behind a
`gui_internal.h` would let readers tell the two apart without
scanning the body.

---

## 4. User-region validation is duplicated across every syscall

`process_user_range_contains` exists and is exercised by
`test_syscall_abi_user_range_validation_rejects_out_of_region`. The
test name is good but the *use* of the helper is scattered: some
syscalls call it directly (sys_write, sys_read), some rely on
`copy_user_cstr` to do the same job (sys_open, sys_spawn_argv,
sys_window_set_title), some don't validate at all
(sys_window_event's buffer copy, sys_window_get_bounds' output
buffer). The host tests cover the helper, not the per-syscall
plumbing.

Symptom: this is exactly how the BSS-static bug went unnoticed. The
shell wrote to unmapped memory because *no syscall ever had to check
that range* — the shell's writes never went through the kernel, the
fault happened on a plain `str` store.

Direction: have every user-pointer-taking syscall funnel through a
shared `copy_user_buf_in` / `copy_user_buf_out` / `copy_user_str_in`.
This also lets `test_syscall_abi.c` swap in a single instrumented
version and lock the contract at one chokepoint instead of 30.

---

## 5. The KLI1 / KOS image-load paths and the bootfs registry
   are scattered

```
kernel/user_image.c        — flat-header parser + loader
kernel/boot_program.c      — bootfs-name -> __app_*_blob symbols
kernel/bootfs.c            — bootfs file table
kernel/panel_boot.c        — load + map the per-process image
kernel/panel_boot_recovery.c  — recovery policy
programs/libkarm/crt0.S    — _start entry
programs/apps/image.ld     — linker script collecting .user_image
programs/apps/*_header.S   — KLI1 header
programs/apps/*_end.S      — image_end marker
```

Six files and two asm files collaborate to produce one loaded process.
The recent BUG 1/2 fixes show what happens when the contract drifts:
`image_end` in the wrong section made every rodata string silently
empty, and no test caught it because the host suite cannot run EL0.

Direction: a single `kernel/user_image_format.h` documenting the KLI1
binary layout (header, sizes, entry offsets, where rodata lives) and
a single owner of the "is the image well-formed" check. `image.ld`
already encodes the layout; mirror that contract into a host test
that builds a fake KLI1 blob with `user_image_load_flat` and
asserts:

- `image_size` covers the entire `.user_image` section,
- the entry offset lands inside the loaded bytes,
- a string at offset `image_size - 1` round-trips correctly.

That last check would have caught the recent BUG 2.

---

## 6. `process_user_region_t` is over-restrictive (4 regions, no
   guard pages)

```c
#define PROCESS_MAX_USER_REGIONS 4U
```

Apps that want "stack + image + scratch + mmap" already hit the
limit, and the host test `test_process_alloc_user_region_respects_region_limit`
locks it in. We have no guard-page handling at all: if a process
runs past the top of its 4 KB stack, the kernel doesn't see a fault
until the address hits another mapping or unmapped memory. That is
how the recent "FAR 0x891000" faults slipped through: the
deeply-nested panel walks off the top of its stack and hits the
next slot's unmapped stack region.

Direction: either bump `PROCESS_MAX_USER_REGIONS` to 6–8 and add
stack-guard support (an unmapped page above each stack), or document
explicitly that 4 KB EL0 stacks are tight and that any state >1 KB
must use the image's rodata / `__asm__(".section .user.image.*")`.

---

## 7. `kernel.c::kernel_main` is a 50-function init sequence

`kernel_main` and its helpers do board init, dtb parse, pmm init,
vmm init, console init, vfs init, FAT32 probe, GPU init, net init,
input init, scheduler init, irq init, panel boot, console loop,
scheduler start. There is no "init phase failed" recovery — if any
step fails the kernel prints a string and falls through to the next
one, so by the time the panel tries to run there is no way to know
which subsystem failed.

Direction: a small `init_status_t` table that each init phase
populates (board, dtb, pmm, vmm, console, vfs, gpu, net, input,
sched, panel). `kernel_main` checks the table once after the boot
sequence and either continues or prints a single "boot failed at
<phase>" line. The same table feeds `k>` so the debug console can
say "sched: not initialised" instead of hanging.

---

## 8. `kernel/console.c` and `kernel/print.c` overlap

Both files implement put-style helpers, both have their own
hex-print routines, both have separate `*_puts` / `*_putc` paths.
`print.c` is freestanding C; `console.c` wraps it for the kernel
debug console (`k>`).

Direction: pick one. The right split is `print.c` (kernel-wide
helpers used by panic / boot) and `console.c` (the line-discipline
loop with history, owned by `k>`). Right now both files have a
`print_hex64` and a `puts`, and the duplication is silent.

---

## 9. Magic numbers in the build / linker script

```
linker.ld:        . = 0x40080000
linker.ld:        . += 0x4000         # __stack_top
kernel/panel_boot.c:  PANEL_BOOT_IMAGE_VA_BASE 0x400000
kernel/panel_boot.c:  PANEL_BOOT_STACK_VA_BASE 0x800000
kernel/panel_boot.c:  PANEL_BOOT_STACK_SIZE   4096
kernel/panel_boot.c:  PANEL_BOOT_IMAGE_SLOT_SIZE 8192
kernel/syscall.c:  USER_FAULT_EXIT_CODE 0xfffffffffffffff0
kernel/exceptions.c: SPSR_EL1H_MASKED 0x3c5
```

These should live in a single `kernel/platform_constants.h` (or
`kernel/layout.h`) with the matching `_Static_assert`s. Right now
the relationship between `0x40080000` and `PANEL_BOOT_IMAGE_VA_BASE
+ PANEL_BOOT_IMAGE_SLOT_SIZE * PROCESS_MAX_PROCESSES` is not
expressed in code, and bumping the image base by accident would
silently overlap the kernel without anyone noticing.

---

## 10. `kernel/fat32.c` is 1128 lines and almost entirely untested
    at the integration level

`test_fat32.c` is thorough about the parser but the FAT32 host test
runs against an in-memory fs. The kernel-side wiring
(`fat32_mount_vfs`, `fat32_set_write_sector`, the read / write
sector plumbing through `board_storage_*`) is never exercised on the
host — only at QEMU runtime, and only when the disk image is
mounted. We have no headless boot test that says "the kernel
parses boot sector / opens / reads / writes / closes a FAT32 file
correctly".

Direction: either expand the host test framework with a fake
storage driver (a `board_storage_*_stub` that returns a fixed
sector), or add a `make qemu-fs-test` target that builds a tiny
FAT32 image, boots QEMU headless with `-drive`, and greps the UART
output for the expected sequence.

---

## 11. Driver headers split between `drivers/...` and `kernel/...`

`drivers/uart/pl011.h`, `drivers/fb/fb.h` etc. are in `drivers/`.
`drivers/storage/virtio_blk.h`, `drivers/gpu/virtio_gpu.h` are in
`drivers/`. `kernel/gui.h`, `kernel/vfs.h`, `kernel/process.h` are
in `kernel/`. There is no documented split rule — it looks like the
rule was "kernel glue in kernel/, hardware in drivers/" but the
boundary leaks: `gui.h` knows about `fb_t` (a drivers/ struct), and
`vfs.h` knows about `fat32_fs_t` (kernel/). Symlink fanout doesn't
help here because the kernel Makefile already has two include paths.

Direction: pick a rule and write it down. Probably:
- `kernel/` — kernel-internal: CPU-state, scheduler, syscall
  dispatch, the platform-agnostic parts of VFS / FS / GUI.
- `drivers/` — anything with hardware registers or transport glue.

Right now `kernel/gui.h` lives in `kernel/` but talks to
`drivers/fb/fb.h`. Either move it to `kernel/gui/` or rename to
`drivers/gui/` so the include graph matches the file graph.

---

## 12. The `k>` debug console command set is a hand-rolled switch
   in `kernel/console.c`

```
$ grep -c "strcmp\|strncmp" kernel/console.c
```

Each command (`help`, `ps`, `mem`, `ticks`, `storage`, `fb`, `mouse`,
`click`) is parsed with a string compare and dispatched. Adding a
command means editing the parser, the help text, and the dispatch.
There is no table to walk.

Direction: define a `struct k_command { const char *name; void (*fn)(void); const char *help; }[]` and iterate it. Also makes `help`
auto-generate from the table instead of being a hand-maintained
printf. ~30-line change, removes a whole class of bugs where the
help text and the dispatch drift apart.

---

## 13. Per-process image-slot BSS is statically allocated

```c
static uint8_t g_user_image_slots[PROCESS_MAX_PROCESSES][PANEL_BOOT_IMAGE_SLOT_SIZE]
    __attribute__((aligned(4096)));
static uint8_t g_user_stacks[PROCESS_MAX_PROCESSES][PANEL_BOOT_STACK_SIZE]
    __attribute__((aligned(4096)));
```

That's `16 * (8192 + 4096) = 196 KB` of static BSS, regardless of
how many processes are alive. For a desktop with one panel plus one
shell that's fine; for the upcoming multiprocess / networking work it
will get tight.

Direction: pull `g_user_image_slots` and `g_user_stacks` out of BSS
into `pmm`-backed allocations. The kernel already has `pmm_alloc_page`
/ `pmm_free_page`. Each spawn could grab a fresh page-aligned block
and each exit frees it. That also fixes the "first fault leaves
stale data in the slot" question raised by BUG 2 (page-aligned
zeroing instead of zero-initialised BSS).

---

## 14. The recovery wrapper's relaunch loop is a good policy but
    a leaky abstraction

`panel_boot_recovery_decide` is pure C and host-tested. Good. But
`panel_boot_run_with_recovery` is `void`-typed except for the
return code, takes `memory_base`, `memory_size`, `map_mmio`
just to forward them, and prints to UART on every iteration. A
cleaner shape:

```c
typedef uint64_t (*panel_boot_run_fn)(void *ctx);
panel_boot_recovery_action_t panel_boot_recovery_run(
    panel_boot_run_fn run, void *ctx,
    void (*log)(const char *line));
```

Then `kernel_main` builds a small `panel_boot_ctx` struct, passes a
log callback, and the recovery module stops pulling in `uart/pl011.h`
directly. Easier to test, easier to disable in release builds,
easier to swap for a "reboot" policy later.

---

## 15. The `panel_state_t` 4 KB stack usage is undocumented

`panel_state_t` is around 450 bytes; `shell_state_t` was ~17 KB and
silently overflowed the kernel's 4 KB stack until the recent fix.
There is no host-side measurement of any app's worst-case stack
frame.

Direction: add a `make stack-check` target that compiles each app
with `-fstack-usage`, sums the per-function stack frames on the
worst-path call chain, and asserts the total < `PANEL_BOOT_STACK_SIZE`.
A single Makefile rule; catches the next "shell static" before it
ships.

---

## 16. We have 30 ROADMAP items ticked but no top-level
    "things that broke last week" trail

The recent QEMU run surfaced three regressions in the same boot.
All three were visible from the UART; none were caught by the
`make -C tests test` suite because the suite does not boot an EL0
app. The host suite is excellent for kernel primitives (process,
vfs, gui, syscall abi) and blind to anything that depends on the
EL0 launch path (image loading, argv placement, per-process stack
plumbing).

Direction: a thin `tests/test_user_image_runtime.c` that uses the
existing `user_image_load_flat` / `kolibri_spawn_vfs` mock to
verify:
- the image_size field covers the entire `.user_image` section
  for each app (would have caught BUG 2),
- `panel_image_vaddr + image_size` does not overlap
  `panel_stack_vaddr`,
- `place_argv_on_stack` rejects strings longer than the budget.

None of these need an actual EL0 image; they run on the host.

---

## 17. `process_remove_user_region` has the wrong ownership
    semantics for the "shrink" path

When `gui_resize_window` runs, the kernel calls
`vmm_unmap_range` on the old backing and reallocates a new one. If
the new allocation fails, the code restores the old mapping via
`gui_move_window`'s `old_size` save. The unmap / realloc / restore
dance is duplicated across `gui_resize_window`,
`gui_destroy_window` (which frees the backing), and the
`user_image_prepare_process` path.

Direction: a `gui_backing_realloc(window, new_size)` that returns
0 / -1 and atomically swaps; if it fails the old backing is still
in place. Reduces three rollback paths to one.

---

## 18. Naming: `_Static_assert` exists everywhere but
    `_static_inline_unused` does not

Several `static` functions in `gui.c` are only used inside one
TU. `-Wunused-function` would warn; the project works around it
with `__attribute__((unused))` ad hoc. A project-level header
with the common pragmas / attributes would remove the per-file
boilerplate.

Direction: a `kernel/kernel_compiler.h` with the project-wide
`__unused`, `__printf_format`, `__packed`, etc., so call sites
read like `static __unused int foo(void)`.

---

## 19. `kernel/net/dhcp.c` ships with hand-rolled byte parsing

`dhcp.c` parses DHCP option fields with manual byte-pointer walks.
There is no host test for DHCP at all (no `test_dhcp.c`), and the
file is not in the test Makefile. The QEMU build can run it, but
the QEMU virt network stack is unstable — the recent boot logs
showed "virtio-net not found" with no clean signal.

Direction: extract the DHCP option parser into a pure function
with a host test that feeds crafted option bytes and asserts the
output struct. That's a 30-line test file and removes the only
runtime-only code path in the kernel.

---

## 20. The `make` output is noisy and hard to scan

Each `make` invocation produces ~80 lines of `gcc` invocations.
There is no `--no-print-directory`, no `-s` shorthand, no
`make help`. A simple `make help` target listing the documented
targets (`qemu`, `qemu-fb`, `qemu-fb-visible`, `qemu-usb`,
`size`, `libkarm`, `apps`, `clean`, `entry-check`) would remove
the "which target runs X?" friction that shows up every time a
new contributor joins.

---

## How to prioritise

The next-batch cleanup should focus on (1) splitting gui.c, (5)
host-testing the KLI1 loader, and (16) testing the EL0 launch path.
Each one closes a class of bugs we just hit (BUG 1/2/3). The other
items are quality-of-life and can be tackled opportunistically.

The very next step after this review should be item (5) + (16): a
small `tests/test_user_image_runtime.c` that proves every shipping
app's image_size covers its rodata and that its argv placement fits
the stack. ~80 lines of host test, two of the three regressions we
just shipped would have been caught before they reached QEMU.