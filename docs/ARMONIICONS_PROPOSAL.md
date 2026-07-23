# ArmoniIcons proposal

> Status: tentative design note. This document does not approve an ABI change,
> select a final icon library, or authorize implementation in a shipping app.

## Purpose

Evaluate a small modern icon system for ArmoniOS without adding an SVG parser,
font engine, general image decoder, or large runtime dependency.

The intended result is a consistent icon vocabulary for `libarmdesk`, the
Panel, menus, buttons, Files, Control, Editor, Monitor, and future launchers.

## Current constraint

The framebuffer implementation already has ARGB pixels, alpha blending, basic
shapes, and bitmap blitting. Userland applications, however, currently draw
through the public GUI text and rectangle operations. The Panel therefore uses
single ASCII characters as temporary app icons.

Any real icon system must bridge that userland drawing gap while remaining
bounded, testable, and compatible with the kernel-size policy.

## Two icon classes

### UI symbols

Small monochrome or duotone symbols, normally 16x16 through 24x24 pixels:

- save, reload, defaults, search, delete, warning;
- folder, file, edit, terminal, settings, activity;
- arrows, disclosure controls, close, minimize, restore.

These should be tintable through semantic theme tokens and are the first
candidate for implementation.

### Application icons

Distinct color icons, normally 32x32 or 48x48 pixels, for applications and
external packages.

These should remain deferred until ArmoniOS has a resource or manifest model.
Embedding many full-color images in compiled-in KLI1 application blobs would
consume kernel-image budget and create duplication.

## Tentative visual source

The preferred design candidate is a curated subset of Lucide-style outline
icons. Tabler and Material Symbols remain alternatives.

No upstream icon set should become a runtime dependency. Before importing any
asset, the project must record the exact source revision, per-asset license,
attribution requirements, local modifications, and generated output.

## Tentative runtime format

For UI symbols, prefer a compact alpha mask rather than full ARGB pixels:

```c
typedef struct {
    uint16_t width;
    uint16_t height;
    const uint8_t *alpha;
} armdesk_icon_t;
```

The mask stores shape coverage while the caller supplies a semantic theme
color. This permits the same asset to render correctly in normal, hovered,
pressed, selected, disabled, light-theme, and dark-theme states.

Possible encodings to measure:

| Encoding | Approximate 20x20 payload | Tradeoff |
|---|---:|---|
| 1-bit mask | 50 bytes | smallest, no antialiasing |
| 4-bit alpha | 200 bytes | compact with useful edge quality |
| 8-bit alpha | 400 bytes | simplest renderer, largest mask |
| ARGB32 | 1600 bytes | suitable only for color app icons |

The final encoding must be selected from measured visual quality, binary size,
and redraw cost rather than preference alone.

## Build-time pipeline

Tentative pipeline:

```text
selected source SVG
        |
        v
host-side conversion and normalization
        |
        v
bounded ArmoniIcons mask data
        |
        v
libarmdesk icon renderer
```

Runtime must not parse SVG, XML, icon fonts, PNG, or floating-point paths.
Generated assets should have deterministic dimensions, names, hashes, and host
tests.

## Drawing-path options

### Option A: rectangle spans, no ABI change

Convert each monochrome icon into horizontal spans and emit existing rectangle
operations.

Advantages:

- no kernel or syscall change;
- useful for a visual prototype and format evaluation.

Disadvantages:

- many syscalls per icon;
- limited antialiasing;
- poor fit for frequent Panel or list redraws.

This is acceptable only as a prototype or fallback.

### Option B: bounded alpha-mask draw operation

Add an append-only GUI operation that receives a caller-owned alpha mask,
position, dimensions, and tint color. The kernel validates ownership, dimensions,
user memory, and byte count, then blends into the window backing buffer.

Tentative constraints:

- fixed maximum dimensions, initially no larger than 64x64;
- checked width-height multiplication and user-copy bounds;
- clipping to the content backing buffer;
- no asset cache, filesystem loading, scaling, rotation, or arbitrary stride in
  the first version;
- normal window damage and flush rules remain unchanged.

This is the preferred production direction only if measurement shows that the
span renderer is too expensive.

## Proposed API shape

Names and syscall numbers are placeholders:

```c
long gui_window_draw_mask(long window_id,
                          long x,
                          long y,
                          long width,
                          long height,
                          long color,
                          const uint8_t *alpha);

void armdesk_icon_draw(long window_id,
                       armdesk_icon_id_t icon,
                       int32_t x,
                       int32_t y,
                       armdesk_theme_token_t color_token);
```

The public `libarmdesk` API should expose stable semantic icon IDs rather than
upstream filenames.

## Tentative initial vocabulary

| Consumer | Semantic icon |
|---|---|
| Shell | terminal |
| Editor | file-edit |
| Files | folder |
| Monitor | activity |
| Control | settings |
| Clock | clock |
| Save action | save |
| Reload action | refresh |
| Defaults action | reset |
| Delete action | trash |
| Search action | search |
| Warning state | warning-triangle |

The application catalog may later carry an `armdesk_icon_id_t`, but that schema
change should occur only when the Panel is ready to consume the icon renderer.

## Proposed delivery cuts

### Cut 1: format and gallery

- choose a small source subset;
- create deterministic host-side conversion;
- define semantic icon IDs and mask format;
- add host tests and a non-shipping visual gallery;
- measure 1-bit, 4-bit, and 8-bit variants;
- do not change the GUI ABI.

### Cut 2: bounded draw path

- choose span rendering or an append-only mask operation from measurements;
- add ownership, clipping, user-copy, overflow, and hostile-input tests;
- record kernel and application size deltas;
- keep icons in userland assets rather than kernel-global UI state.

### Cut 3: Panel adoption

- add icon IDs to visible app metadata;
- replace temporary ASCII app markers;
- verify focus, running, minimized, hover, and instance indicators;
- measure redraw cost and visible quality at 20x20 or 22x22.

Application-wide adoption should occur only after the Panel proves the path.

## Decision gates

Implementation should not start until all of these are answered:

1. Which exact icon source and revision are approved?
2. Which mask encoding meets the size and quality target?
3. Can the existing rectangle path meet Panel redraw requirements?
4. Is a new GUI operation justified by measured syscall cost?
5. Where do generated assets live before external app resources exist?
6. What is the maximum icon dimension and byte count?
7. How are source, license, attribution, and generated hashes recorded?
8. What are the kernel, Control, Panel, and total embedded-image deltas?

## Explicit non-goals

- runtime SVG or vector-path rendering;
- icon fonts;
- general PNG/JPEG decoding;
- arbitrary image scaling or rotation;
- animation;
- color application-icon packaging before manifests/resources;
- adding icon behavior to the current Control widget PR;
- importing an entire upstream icon collection.

## Recommendation

Proceed later with a small monochrome UI-symbol experiment, using a curated
outline set and a tintable mask format. Keep the design tentative until a
visual gallery and size/performance measurements demonstrate that it is worth
an ABI addition. Full-color application icons should remain deferred to the
resource and manifest roadmap.
