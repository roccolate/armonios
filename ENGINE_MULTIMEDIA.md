# Engine and Multimedia Plan

This document captures the accepted long-term direction for graphics, input,
audio, resources, and interactive multimedia applications in KolibriARM.

Status: accepted as a post-v1.5 direction. It does not replace the current
loader, filesystem, GUI, networking, or Raspberry Pi port milestones in
`ROADMAP.md`.

## Integration Rules

- Keep the hot path in C. Code that touches pixels, mixes audio, handles device
  interrupts, or copies buffers stays in C or small AArch64 assembly helpers.
- Scripting, if added, calls C APIs. Kernel code must not call into scripts.
- Do not put Lua or any other hosted runtime in the kernel. A script runtime can
  only be considered later as a userland/runtime library after VFS, loader,
  input, display, and audio are stable.
- Keep assets out of C code once the VFS exists. During bootstrap, embedded test
  assets are allowed, but production sprites, tilemaps, fonts, and audio should
  be loaded through VFS/resource handles.
- Do not allocate from the kernel heap inside a frame loop, input IRQ hot path,
  or audio callback. Subsystems initialize fixed pools up front and manage their
  own pool state.
- Measure before optimizing. Scalar C implementations come first with host
  tests; NEON fast paths are added only after the behavior is covered.
- Keep board-specific display, input, storage, and audio details in board/driver
  layers. Generic engine code must not depend on QEMU or Raspberry Pi constants.

## Frame Budget Target

The first interactive target is 60 FPS, which gives a 16.6 ms frame. These
numbers are planning budgets, not hard ABI guarantees:

| Area | Budget |
|------|--------|
| App logic / scripting | 2 ms |
| Compositor / render traversal | 6 ms |
| Asset blits | 3 ms |
| Audio mix | 2 ms |
| Buffer flip / present | 1 ms |
| Margin | 2 ms |

A simple profiler should land before serious engine optimization so each
subsystem can report its per-frame cost.

## Phase 8 - Userland C SDK

Before adding larger tools or multimedia demos, introduce a tiny native C SDK for
KolibriARM user applications. The goal is not POSIX compatibility yet; the goal
is to stop writing large user apps directly in AArch64 assembly and to give
future tools a stable syscall and GUI wrapper layer.

Initial SDK tree:

```text
sdk/
  crt0.S
  syscalls.h
  syscall.S
  kolibriarm.h
  gui.c
  gui.h
  string.c
  app.ld
```

Initial C app examples:

```text
programs/apps_c/hello_c/main.c
programs/apps_c/clock/main.c
```

Initial scope:

- `crt0.S` provides the user entry point, calls `main(argc, argv)`, then exits
  through `SYS_EXIT`.
- `syscall.S` provides a small generic syscall trampoline using `svc #0`.
- `syscalls.h` defines syscall numbers and kernel ABI constants in one place.
- `kolibriarm.h` exposes common userland helpers such as `ka_exit`, `ka_yield`,
  `ka_spawn`, `ka_getpid`, `ka_read`, `ka_write`, `ka_timeinfo`, `ka_meminfo`,
  and `ka_proclist`.
- `gui.h` / `gui.c` wrap the window syscalls into safer app-facing helpers such
  as `ka_window_create`, `ka_window_destroy`, `ka_draw_text`, `ka_draw_rect`,
  `ka_window_event`, `ka_window_redraw`, and `ka_window_focus`.
- `string.c` provides only the tiny routines needed by bootstrap apps, such as
  `strlen`, `strcmp`, `strncmp`, `memcpy`, `memset`, and integer formatting.
- `app.ld` defines the user app image layout expected by the current loader.
- Keep the SDK freestanding: no libc dependency until the app ABI, VFS, loader,
  and memory model are stable.

Candidate API shape:

```c
int ka_window_create(int x, int y, int w, int h, const char *title);
void ka_draw_text(int win, int x, int y, uint32_t color, const char *text);
void ka_draw_rect(int win, int x, int y, int w, int h, uint32_t color);
int ka_window_event(int win, ka_event_t *events, int max_events);
void ka_window_redraw(int win);
int ka_spawn(const char *path);
void ka_yield(void);
void ka_exit(int code);
```

Migration rule:

- Keep tiny smoke-test apps in assembly (`hello`, `loop`, `fault`) because they
  are useful for ABI and exception testing.
- New useful tools should default to C once the SDK exists.
- Existing large assembly apps (`panel`, `shell`, `editor`, `monitor`) should be
  stabilized first, then gradually rewritten or replaced with C versions.
- Kernel low-level entry code, exception vectors, context switching, EL0 entry,
  and `crt0.S` remain assembly.

This SDK is a prerequisite for the multimedia phases below: the engine should be
used from C apps, not from handwritten syscall-heavy assembly.

## Phase 9 - Display Backbone

Build on the existing `drivers/fb` and `virtio-gpu` work instead of replacing
it.

Initial scope:
- Front/back framebuffer ownership and an explicit present path.
- QEMU `virtio-gpu` present path first; Raspberry Pi display support stays
  behind the board layer.
- Timer-driven 16 ms present pacing in QEMU until a real vsync source exists.
- Logical resolution support for small internal canvases scaled to the visible
  framebuffer.
- Clear cacheability rules for framebuffer memory before adding faster blits.

Candidate API shape:

```c
int fb_init(uint32_t width, uint32_t height);
void fb_clear(uint32_t color);
void fb_flip(void);
uint32_t *fb_backbuffer(void);
```

## Phase 10 - 2D Graphics Primitives

Add a small clipped 2D drawing layer over the framebuffer.

Initial scope:
- `gfx_rect`, `gfx_fill`, `gfx_line`, and basic circle support.
- Clipped blits and color-key blits.
- Built-in bitmap font for bootstrap GUI text; VFS-loaded fonts can replace it
  once the resource manager exists.
- ARGB8888 as the generic software color format.
- Host tests for clipping and edge cases before any NEON path is added.

Candidate API shape:

```c
void gfx_rect(int x, int y, int w, int h, uint32_t color);
void gfx_fill(int x, int y, int w, int h, uint32_t color);
void gfx_line(int x0, int y0, int x1, int y1, uint32_t color);
void gfx_blit(const sprite_t *src, int dx, int dy);
void gfx_blit_key(const sprite_t *src, int dx, int dy, uint32_t key);
void gfx_text(int x, int y, const char *str, uint32_t color);
```

## Phase 11 - Input Layer

Unify keyboard, mouse, and gamepad state behind a small event queue.

Initial scope:
- Keep UART console input as the bootstrap keyboard path.
- Add an input event ring that can be filled by UART, virtio-input, or USB HID.
- QEMU uses virtio-input before Raspberry Pi USB HID work.
- Track current and previous button/key state so real-time apps can ask for
  held and newly-pressed inputs.

Candidate API shape:

```c
bool input_key_down(uint8_t keycode);
bool input_key_pressed(uint8_t keycode);
bool input_btn(uint8_t pad, uint8_t button);
bool input_poll_event(input_event_t *event);
```

## Phase 12 - Audio

Add software-mixed PCM output only after timer, IRQ, storage, and resource
loading are reliable enough to feed it.

Initial scope:
- QEMU target: virtio-sound if the environment supports it.
- Raspberry Pi target: I2S/HDMI through a board-specific driver.
- PCM 16-bit signed stereo at 44.1 kHz for the first mixer.
- Fixed-point channel mixing with clamp to signed 16-bit.
- No allocation in the IRQ/audio callback.

Candidate API shape:

```c
void audio_init(uint32_t sample_rate, uint8_t channels);
audio_handle_t audio_play(const audio_buf_t *buf, bool loop);
void audio_stop(audio_handle_t handle);
void audio_set_volume(audio_handle_t handle, uint8_t volume);
void audio_mix_callback(int16_t *out, uint32_t frames);
```

## Phase 13 - VFS Resource Manager

Layer asset loading on top of the Phase 4 VFS rather than inventing a separate
storage path.

Initial scope:
- Load sprites, tilemaps, bitmap fonts, and audio buffers from VFS handles.
- Cache resources with explicit size limits and deterministic eviction.
- Keep loaded assets in non-pageable memory while they are active.
- Use simple binary formats with magic values and fixed-width fields.

Initial sprite format:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Magic `KSPR` |
| 4 | 2 | Sheet width |
| 6 | 2 | Sheet height |
| 8 | 2 | Frame width |
| 10 | 2 | Frame height |
| 12 | 4 | Color key |
| 16 | N | Pixel data |

Initial tilemap format:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Magic `KTLM` |
| 4 | 2 | Columns |
| 6 | 2 | Rows |
| 8 | 2 | Tile width |
| 10 | 2 | Tile height |
| 12 | 4 | Sprite sheet ID |
| 16 | N | `uint16_t` tile IDs, row-major |

## Phase 14 - Compositor

Align this with the existing Phase 5 GUI plan.

Initial scope:
- Layer/window composition over the framebuffer backbuffer.
- Dirty rectangle tracking for unchanged regions.
- Cursor sprite and hotspot handling.
- Scroll offsets for tilemap-like backgrounds where useful.
- Input events routed to the focused window/process through the GUI event
  path, not direct driver calls from applications.

## Phase 15 - Interactive Runtime

Build a small app loop and multimedia helpers on top of display, input, audio,
VFS, and the compositor.

Initial scope:
- Fixed timestep app loop for demos and educational apps.
- Collision primitives in C (`AABB`, point-rect, tile solidity checks).
- Optional entity pool with fixed capacity.
- Basic sequencer for structured music only after the sample mixer works.
- Optional scripting after the C runtime is stable and after the memory/runtime
  costs are measured.

Candidate app loop shape:

```c
typedef void (*app_init_fn)(void);
typedef void (*app_update_fn)(uint32_t tick);
typedef void (*app_draw_fn)(void);

typedef struct {
    app_init_fn init;
    app_update_fn update;
    app_draw_fn draw;
    uint32_t target_fps;
} app_desc_t;

void app_run(const app_desc_t *desc);
```

## Hardware Order

| Platform | Role |
|----------|------|
| QEMU `virt` | Primary development path |
| Raspberry Pi 4/5 | First real hardware target from the current roadmap |
| Raspberry Pi 3B/3B+ / Zero 2W | Later constrained-hardware validation |

The proposal's Raspberry Pi 3/Zero targets are useful constraints, but the
current roadmap already targets Raspberry Pi 4/5 first. Keep that order unless
the board abstraction work shows a cheaper path to Pi 3-class hardware.
