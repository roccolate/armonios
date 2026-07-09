# Engine and Multimedia Plan

This document captures the accepted long-term direction for graphics, input,
audio, resources, and interactive multimedia applications in ArmoniOS.

Status: future-facing design guidance for the minimal engine track and the
later v2.0 engine/multimedia runtime. It does not replace the current loader,
filesystem, GUI, networking, QEMU desktop stabilization, or Raspberry Pi port
milestones in `ROADMAP.md`.

Current rule: the first engine work must stay in userland unless a demo proves a
missing kernel capability. Do not add new kernel graphics, audio, or input
syscalls speculatively.

## Integration Rules

- Keep the hot path in C. Code that touches pixels, mixes audio, handles device
  interrupts, or copies buffers stays in C or small AArch64 assembly helpers.
- Scripting, if added, calls C APIs. Kernel code must not call into scripts.
- Do not put Lua or any other hosted runtime in the kernel. A script runtime can
  only be considered later as a userland/runtime library after VFS, loader,
  input, display, and audio are stable.
- Keep assets out of C code once the VFS path is sufficient. During bootstrap,
  embedded test assets are allowed, but production sprites, tilemaps, fonts, and
  audio should be loaded through VFS/resource handles.
- Do not allocate from the kernel heap inside a frame loop, input IRQ hot path,
  or audio callback. Subsystems initialize fixed pools up front and manage their
  own pool state.
- Measure before optimizing. Scalar C implementations come first with host
  tests; NEON fast paths are added only after the behavior is covered.
- Keep board-specific display, input, storage, and audio details in board/driver
  layers. Generic engine code must not depend on QEMU or Raspberry Pi constants.
- Build on `programs/libkarm` and `programs/libkarmdesk`; do not create a second
  SDK tree without a proved need.

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

## Current Userland SDK Baseline

This prerequisite is complete enough for minimal userland experiments:

- `programs/libkarm/` provides user entry, syscall trampolines, and small
  freestanding C helpers.
- `programs/libkarmdesk/` provides app-facing desktop/window wrappers.
- Shipping apps are C programs under `programs/apps/`: panel, shell, editor,
  files, monitor, and clock.
- The app linker script emits KLI1 flat images consumed by the current loader.
- Kernel low-level entry code, exception vectors, context switching, EL0 entry,
  and `crt0.S` remain assembly.
- Input from UART, virtio-input, and USB HID feeds the common input queue before
  reaching the GUI/window event path.
- VFS exposes bootfs, tmpfs, and the current FAT32 root-file workflow.

Future engine APIs should reuse this baseline and remain small enough that app
image sizes and `make stack-check` stay green.

## Minimal Engine Track

Start this only after the desktop-core gates pass. The first implementation
should be a small userland helper layer or a tiny demo app.

Allowed first steps:

- a compact app-loop helper over existing `libkarm` / `libkarmdesk` calls;
- fixed timestep update/draw loop;
- simple timing helper based on the existing system-info syscall surface;
- owner-window drawing helpers that use current desktop ABI calls;
- VFS asset-loading conventions for tiny binary assets;
- a demo that proves what the current ABI can and cannot do.

Avoid in this track:

- new kernel graphics syscalls;
- audio claims;
- new input syscalls;
- broad compositor rewrites;
- scripting runtimes;
- asset pipelines that require hosted OS features.

Exit signal:

- a demo runs as a normal EL0 app;
- release gates remain green;
- any missing kernel capability is described with a concrete demo failure, not a
  speculative design preference.

## Display Backbone

Build on the existing `drivers/fb`, `virtio-gpu`, and kernel GUI work instead of
replacing it.

Initial scope:

- userland helper API over the current owner-window draw/event calls;
- damage-aware redraw behavior through existing window syscalls;
- QEMU `virtio-gpu` as the primary development path;
- Raspberry Pi display support behind the board layer later;
- logical resolution helpers for small internal canvases scaled into a window;
- clear cacheability rules before adding faster blits or mapped buffers.

Candidate API shape for a later helper library:

```c
int karm_app_init(uint32_t width, uint32_t height, const char *title);
void karm_clear(uint32_t color);
void karm_present(void);
uint32_t *karm_backbuffer(void);
```

Do not treat this candidate API as implemented until a demo lands.

## 2D Graphics Primitives

Add a small clipped 2D drawing layer over the existing userland/window surface.

Initial scope:

- filled rectangles, lines, simple circles;
- clipped blits and color-key blits;
- bitmap font helpers only if they reduce app duplication;
- ARGB8888 as the generic software color format;
- host tests for clipping and edge cases before any NEON path is added.

Candidate API shape:

```c
void gfx_rect(int x, int y, int w, int h, uint32_t color);
void gfx_fill(int x, int y, int w, int h, uint32_t color);
void gfx_line(int x0, int y0, int x1, int y1, uint32_t color);
void gfx_blit(const sprite_t *src, int dx, int dy);
void gfx_blit_key(const sprite_t *src, int dx, int dy, uint32_t key);
void gfx_text(int x, int y, const char *str, uint32_t color);
```

## Input Layer

The kernel already has UART, virtio-input, and USB HID paths feeding a common
input queue. Engine work should not replace that path.

Initial app-facing scope:

- translate current GUI/window events into held/pressed/released helper state;
- keep current and previous button/key state for real-time apps;
- add gamepad concepts only after a real driver path exists;
- keep direct driver calls out of applications.

Candidate API shape:

```c
bool input_key_down(uint8_t keycode);
bool input_key_pressed(uint8_t keycode);
bool input_btn(uint8_t pad, uint8_t button);
bool input_poll_event(input_event_t *event);
```

## Audio

Audio is future work. Add software-mixed PCM output only after timer, IRQ,
storage, and resource loading are reliable enough to feed it.

Initial scope:

- QEMU target: virtio-sound only if the environment and documentation make it a
  small, testable driver path;
- Raspberry Pi target: board-specific I2S/HDMI path;
- PCM 16-bit signed stereo at 44.1 kHz for the first mixer;
- fixed-point channel mixing with clamp to signed 16-bit;
- no allocation in the IRQ/audio callback.

Candidate API shape:

```c
void audio_init(uint32_t sample_rate, uint8_t channels);
audio_handle_t audio_play(const audio_buf_t *buf, bool loop);
void audio_stop(audio_handle_t handle);
void audio_set_volume(audio_handle_t handle, uint8_t volume);
void audio_mix_callback(int16_t *out, uint32_t frames);
```

## VFS Resource Manager

Layer asset loading on top of VFS rather than inventing a separate storage path.

Initial scope:

- load sprites, tilemaps, bitmap fonts, and audio buffers from VFS handles;
- cache resources with explicit size limits and deterministic eviction;
- keep loaded assets in non-pageable memory while they are active;
- use simple binary formats with magic values and fixed-width fields.

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

## Compositor Direction

Build on the current kernel GUI split rather than replacing it.

Initial scope after the minimal userland demo proves the need:

- layer/window composition improvements over existing backing buffers;
- dirty rectangle tracking for unchanged regions;
- cursor sprite and hotspot handling;
- scroll offsets for tilemap-like backgrounds where useful;
- input events routed to the focused window/process through the GUI event path,
  not direct driver calls from applications.

## Interactive Runtime

Build a small app loop and multimedia helpers on top of display, input, audio,
VFS, and the compositor after the stable desktop base exists.

Initial scope:

- fixed timestep app loop for demos and educational apps;
- collision primitives in C (`AABB`, point-rect, tile solidity checks);
- optional entity pool with fixed capacity;
- basic sequencer for structured music only after the sample mixer works;
- optional scripting after the C runtime is stable and after memory/runtime costs
  are measured.

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
| QEMU `virt` | Primary development path and v1.0 release target |
| Raspberry Pi 4/5 | First real hardware target from the current roadmap |
| Raspberry Pi 3B/3B+ / Zero 2W | Later constrained-hardware validation |

The proposal's Raspberry Pi 3/Zero targets are useful constraints, but the
current roadmap targets Raspberry Pi 4/5 first. Keep that order unless the board
abstraction work shows a cheaper path to Pi 3-class hardware.
