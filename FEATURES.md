# Future Features

This file records small feature ideas that are worth preserving but are not part
of the current promoted milestone. It is not a roadmap or a statement of
implemented behavior.

## Pixel-art icons and emoticons

**Status:** deferred for a later GUI/userland milestone.

ArmoniOS should use native bitmap pixel art instead of full Unicode emoji.
The current text renderer supports only printable ASCII, and a complete emoji
font/Unicode stack would add disproportionate complexity and size.

Proposed direction:

- Create a small, consistent icon set named **ArmoniIcons**.
- Start with monochrome 8x8 icons; add 16x16 icons where the UI needs more detail.
- Initial set: folder, file, terminal, settings, info, warning, error, smile, sad,
  wink, and heart.
- Store each icon as a compact 1-bit bitmap/mask with explicit width and height.
- Provide ASCII fallbacks such as `:)`, `:(`, `;)`, and `<3`.
- Keep the first implementation in userland/libkarmdesk where possible.
- Later add one generic bitmap or mask drawing syscall rather than an
  emoji-specific syscall.
- Treat UTF-8 text support as a separate feature; do not require it for icons.

Possible API shape:

```c
typedef struct {
    const uint8_t *bits;
    uint8_t width;
    uint8_t height;
} gui_icon_t;

long gui_window_draw_icon(long window_id, long x, long y,
                          long color, const gui_icon_t *icon);
```

Before implementation:

1. Finish and promote the current milestone.
2. Recheck kernel and embedded-app size budgets.
3. Decide whether icon data lives in applications, a shared userland runtime,
   or a generic kernel-assisted mask renderer.
4. Add a small icon gallery/demo and visible QEMU verification.
