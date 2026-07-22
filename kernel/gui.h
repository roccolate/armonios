#ifndef ARMONIOS_KERNEL_GUI_H
#define ARMONIOS_KERNEL_GUI_H

#include <stdint.h>

#include "include/armonios/abi/gui.h"
#include "input/input.h"

typedef struct fb fb_t;

#define GUI_MAX_WINDOWS      16U
#define GUI_NO_WINDOW        0xffffffffU
#define GUI_CURSOR_W         16
#define GUI_CURSOR_H         16
#define GUI_TITLE_LEN        32U
/* Cap on tracked damage rectangles per desktop. When the list fills we
 * collapse it to a single "full screen" sentinel, which keeps the worst
 * case identical to the pre-damage path. 32 covers the common
 * per-rect redraw bursts from one or two apps without spilling. */
#define GUI_DAMAGE_MAX       32U
/* Kernel-drawn close button inside the title bar. The box is only
 * rendered (and only intercepts clicks) when the window has a
 * title_h large enough to fit it without crowding the title text. */
#define GUI_CLOSE_BTN_W      14U
#define GUI_CLOSE_BTN_PAD     2U
#define GUI_CLOSE_BTN_MIN_TITLE_H 10U
/* Default kernel-drawn title bar height when an app requests one. Fits
 * the 5x7 bitmap font (7 px glyph) plus 5 px of vertical padding. */
#define GUI_TITLE_BAR_H      12U
#define GUI_EVENT_QUEUE_SIZE 32U
#define GUI_NO_OWNER         0xffffffffU

#define GUI_WINDOW_NO_FOCUS     (1U << 0)
#define GUI_WINDOW_NO_DRAG      (1U << 1)
#define GUI_WINDOW_SKIP_TASKBAR (1U << 2)
#define GUI_WINDOW_DOCK         (1U << 3)

/* Cap on per-window cursor-shape regions. Each registered region is a
 * (x, y, w, h, shape) tuple in the window's content-local coords; the
 * kernel walks them in slot order on every cursor move and the first
 * region that contains the cursor wins over the kernel's title-bar
 * default. 8 covers a normal launcher/taskbar/widget surface without
 * spilling. */
#define GUI_MAX_CURSOR_REGIONS  8U

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t bg_color;
    uint32_t border_color;
    uint32_t owner_pid;
    /* Larger z values are drawn and hit-tested above smaller values.
     * Window ids remain stable pool indices; focusing raises by bumping
     * this field, never by moving window structs. */
    uint32_t z;
    /* Per-window policy flags. These describe window-manager behavior, not
     * drawing. For example, a dock/taskbar should receive mouse events but
     * should not become the focused application window or be draggable. */
    uint32_t flags;
    /* Kernel-drawn title bar height in pixels. 0 means no title bar.
     * When set, the kernel paints a solid bar at the top of the window
     * and draws the title text inside it during gui_draw_window. Owner
     * drawing via SYS_WINDOW_DRAW_RECT/TEXT has its y coordinate shifted
     * down by title_h so apps keep a clean 0-based content coordinate
     * space below the bar. */
    uint32_t title_h;
    char title[GUI_TITLE_LEN];
    gui_event_t events[GUI_EVENT_QUEUE_SIZE];
    uint32_t event_head;
    uint32_t event_tail;
    uint32_t event_count;
    uint8_t used;
    /* Set when an EL0 owner has drawn into this window via
     * SYS_WINDOW_DRAW_RECT or SYS_WINDOW_DRAW_TEXT. When set, the kernel
     * compositor skips the bg_color fillrect on redraw and instead
     * blits the window's backing buffer onto the framebuffer. The owner
     * is responsible for painting its own background the first time.
     */
    uint8_t owner_drawn;
    /* Set when the user clicked the kernel-drawn minimise button. The
     * compositor skips minimised windows entirely; the panel greys
     * the corresponding running-apps slot and clicking that slot
     * restores the window via gui_window_restore. The owner learns
     * about the transition through GUI_EVENT_MINIMIZE / GUI_EVENT_MAXIMIZE
     * on the event queue; the kernel-side flag is the source of truth. */
    uint8_t minimized;
    /* Per-window backing buffer. Allocated lazily on the first owner
     * draw; covers only the content area (excluding the kernel-drawn
     * title bar). Width = window->w, height = window->h - title_h. When
     * non-NULL, the compositor blits this buffer at (window.x,
     * window.y + title_h) during gui_draw_window, so dragging or z-order
     * changes keep the content attached to the window instead of
     * leaving it stranded at the previous framebuffer position. */
    uint32_t *backing;
    uint32_t backing_capacity;
    /* Per-window cursor-shape regions. Each slot is either inactive
     * (used == 0) or describes a content-local rectangle that should
     * show `shape` while the cursor is inside. The kernel walks the
     * slots in ascending index order during gui_refresh_cursor_shape
     * and uses the first region that contains the cursor; if none
     * matches it falls back to the title-bar / arrow default. */
    struct {
        int32_t x;
        int32_t y;
        int32_t w;
        int32_t h;
        uint32_t shape;
        uint8_t used;
    } cursor_regions[GUI_MAX_CURSOR_REGIONS];
} gui_window_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t prev_x;
    int32_t prev_y;
    uint8_t buttons_mask;
    uint8_t visible;
    uint8_t shape;
} gui_cursor_t;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} damage_rect_t;

typedef struct {
    fb_t *fb;
    uint32_t background_color;
    uint32_t focused_window_id;
    uint32_t next_z;
    /* Drag state. drag_window_id == GUI_NO_WINDOW when no drag is active.
     * drag_off_x/off_y is the cursor offset from the window's top-left at
     * the start of the drag; while dragging, gui_drag_update moves the
     * window to keep that offset constant under the cursor. */
    uint32_t drag_window_id;
    int32_t drag_off_x;
    int32_t drag_off_y;
    gui_window_t windows[GUI_MAX_WINDOWS];
    gui_cursor_t cursor;
    /* Damage tracking. The compositor walks damage_rects on the next redraw
     * and only repaints those regions. When damage_full is set the list is
     * ignored and the full framebuffer is repainted (cheaper than a huge
     * rect list once the bursts accumulate). Coords are in framebuffer
     * pixels. */
    damage_rect_t damage_rects[GUI_DAMAGE_MAX];
    uint32_t damage_count;
    uint8_t damage_full;
} gui_desktop_t;

#include "kernel/gui_events.h"
#include "kernel/gui_cursor.h"
#include "kernel/gui_input.h"
#include "kernel/gui_pool.h"
#include "kernel/gui_compositor.h"

#endif
