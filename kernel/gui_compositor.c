#include "kernel/gui_compositor.h"

#include <stdint.h>

#include "fb/fb.h"
#include "kernel/font.h"
#include "kernel/gui_backing.h"
#include "kernel/gui_internal.h"

static fb_t g_gui_fb;
static gui_desktop_t g_gui_desktop;
static uint8_t g_gui_active;
static uint8_t g_gui_dirty;

static const uint8_t g_cursor[GUI_CURSOR_H][GUI_CURSOR_W] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0},
    {1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
    {1, 2, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0},
    {1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0},
};

static const uint8_t g_cursor_hand[GUI_CURSOR_H][GUI_CURSOR_W] = {
    {0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 2, 2, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 2, 2, 1, 1, 2, 2, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 2, 2, 1, 2, 2, 2, 1, 1, 0, 0, 0, 0},
    {0, 0, 0, 1, 2, 2, 1, 2, 2, 2, 1, 2, 1, 0, 0, 0},
    {0, 0, 1, 1, 2, 2, 1, 2, 2, 2, 1, 2, 1, 0, 0, 0},
    {0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 0, 0, 0},
    {0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0},
    {0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0},
    {0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0},
    {0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0},
};

static uint32_t gui_blend_color(uint32_t top, uint32_t bottom,
                                uint32_t num, uint32_t denom) {
    uint32_t tr = (top >> 16) & 0xffU;
    uint32_t tg = (top >> 8) & 0xffU;
    uint32_t tb = top & 0xffU;
    uint32_t br = (bottom >> 16) & 0xffU;
    uint32_t bg = (bottom >> 8) & 0xffU;
    uint32_t bb = bottom & 0xffU;
    uint32_t r;
    uint32_t g;
    uint32_t b;

    if (denom == 0U) {
        return top;
    }
    if (num >= denom) {
        return (top & 0xff000000U) | (br << 16) | (bg << 8) | bb;
    }

    r = (uint32_t)((int32_t)tr +
                   (((int32_t)br - (int32_t)tr) * (int32_t)num) /
                       (int32_t)denom);
    g = (uint32_t)((int32_t)tg +
                   (((int32_t)bg - (int32_t)tg) * (int32_t)num) /
                       (int32_t)denom);
    b = (uint32_t)((int32_t)tb +
                   (((int32_t)bb - (int32_t)tb) * (int32_t)num) /
                       (int32_t)denom);

    return (top & 0xff000000U) | (r << 16) | (g << 8) | b;
}

static void gui_draw_cursor(fb_t *fb, int32_t x, int32_t y, uint8_t shape) {
    const uint8_t (*bitmap)[GUI_CURSOR_W] =
        (shape == GUI_CURSOR_HAND) ? g_cursor_hand : g_cursor;

    for (uint32_t row = 0; row < GUI_CURSOR_H; row++) {
        for (uint32_t col = 0; col < GUI_CURSOR_W; col++) {
            uint8_t v = bitmap[row][col];
            if (v == 0U) {
                continue;
            }
            fb_putpixel(fb, (uint32_t)(x + (int32_t)col),
                        (uint32_t)(y + (int32_t)row),
                        v == 1U ? 0xff101010U : 0xffe0e0e0U);
        }
    }
}

static int gui_rects_overlap(int32_t ax, int32_t ay, int32_t aw, int32_t ah,
                             int32_t bx, int32_t by, int32_t bw, int32_t bh) {
    return ax + aw > bx && ax < bx + bw && ay + ah > by && ay < by + bh;
}

static void gui_fillrect_clipped(fb_t *fb, int32_t x, int32_t y, int32_t w,
                                 int32_t h, int32_t cx0, int32_t cy0,
                                 int32_t cx1, int32_t cy1, uint32_t color) {
    int32_t x1 = x + w;
    int32_t y1 = y + h;

    if (w <= 0 || h <= 0) {
        return;
    }
    if (x < cx0) {
        x = cx0;
    }
    if (y < cy0) {
        y = cy0;
    }
    if (x1 > cx1) {
        x1 = cx1;
    }
    if (y1 > cy1) {
        y1 = cy1;
    }
    if (x1 <= x || y1 <= y) {
        return;
    }
    fb_fillrect(fb, (uint32_t)x, (uint32_t)y,
                (uint32_t)(x1 - x), (uint32_t)(y1 - y), color);
}

static int32_t gui_iabs32(int32_t value) {
    return value < 0 ? -value : value;
}

static void gui_putpixel_clipped(fb_t *fb, int32_t x, int32_t y,
                                 int32_t cx0, int32_t cy0, int32_t cx1,
                                 int32_t cy1, uint32_t color) {
    if (x < cx0 || x >= cx1 || y < cy0 || y >= cy1 ||
        x < 0 || y < 0) {
        return;
    }

    fb_putpixel(fb, (uint32_t)x, (uint32_t)y, color);
}

static void gui_draw_line_clipped(fb_t *fb, int32_t x0, int32_t y0,
                                  int32_t x1, int32_t y1, int32_t cx0,
                                  int32_t cy0, int32_t cx1, int32_t cy1,
                                  uint32_t color) {
    int32_t dx = gui_iabs32(x1 - x0);
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t dy = -gui_iabs32(y1 - y0);
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;

    for (;;) {
        int32_t e2;

        gui_putpixel_clipped(fb, x0, y0, cx0, cy0, cx1, cy1, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }

        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void gui_draw_rect_clipped(fb_t *fb, int32_t x, int32_t y, int32_t w,
                                  int32_t h, int32_t cx0, int32_t cy0,
                                  int32_t cx1, int32_t cy1,
                                  uint32_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }

    gui_fillrect_clipped(fb, x, y, w, 1, cx0, cy0, cx1, cy1, color);
    if (h > 1) {
        gui_fillrect_clipped(fb, x, y + h - 1, w, 1,
                             cx0, cy0, cx1, cy1, color);
    }
    if (h > 2) {
        gui_fillrect_clipped(fb, x, y + 1, 1, h - 2,
                             cx0, cy0, cx1, cy1, color);
        if (w > 1) {
            gui_fillrect_clipped(fb, x + w - 1, y + 1, 1, h - 2,
                                 cx0, cy0, cx1, cy1, color);
        }
    }
}

static void gui_blit_clipped(fb_t *fb, int32_t dst_x, int32_t dst_y,
                             const uint32_t *src, uint32_t src_w,
                             uint32_t src_h, int32_t cx0, int32_t cy0,
                             int32_t cx1, int32_t cy1) {
    int32_t x0 = dst_x;
    int32_t y0 = dst_y;
    int32_t x1 = dst_x + (int32_t)src_w;
    int32_t y1 = dst_y + (int32_t)src_h;
    uint32_t src_off_x;
    uint32_t src_off_y;

    if (fb == 0 || src == 0 || src_w == 0U || src_h == 0U) {
        return;
    }
    if (x0 < cx0) {
        x0 = cx0;
    }
    if (y0 < cy0) {
        y0 = cy0;
    }
    if (x1 > cx1) {
        x1 = cx1;
    }
    if (y1 > cy1) {
        y1 = cy1;
    }
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    src_off_x = (uint32_t)(x0 - dst_x);
    src_off_y = (uint32_t)(y0 - dst_y);
    for (int32_t row = y0; row < y1; row++) {
        uint32_t dst_row = (uint32_t)row * fb->stride_pixels + (uint32_t)x0;
        uint32_t src_row = (src_off_y + (uint32_t)(row - y0)) * src_w +
                           src_off_x;
        for (int32_t col = x0; col < x1; col++) {
            fb->pixels[dst_row + (uint32_t)(col - x0)] =
                src[src_row + (uint32_t)(col - x0)];
        }
    }
}

static void gui_draw_window_title(fb_t *fb, const gui_desktop_t *desktop,
                                  uint32_t index,
                                  const gui_window_t *window,
                                  uint32_t border, int32_t cx0, int32_t cy0,
                                  int32_t cx1, int32_t cy1) {
    uint32_t bar_color;
    uint32_t text_y;
    uint32_t mb_x = 0, mb_y = 0, mb_w = 0, mb_h = 0;
    uint32_t xb_x = 0, xb_y = 0, xb_w = 0, xb_h = 0;
    uint32_t cb_x = 0, cb_y = 0, cb_w = 0, cb_h = 0;
    uint32_t min_bg;
    uint32_t max_bg;
    uint32_t cls_bg;

    if (window->title_h == 0U || window->title[0] == '\0') {
        return;
    }

    bar_color = gui_blend_color(border, 0xff000000U, 2U, 3U);
    text_y = window->y +
             (window->title_h > FONT_GLYPH_HEIGHT
                  ? (window->title_h - FONT_GLYPH_HEIGHT) / 2U
                  : 0U);

    gui_fillrect_clipped(fb, (int32_t)window->x, (int32_t)window->y,
                         (int32_t)window->w, (int32_t)window->title_h,
                         cx0, cy0, cx1, cy1, bar_color);
    if (window->title_h > 0U) {
        gui_fillrect_clipped(fb, (int32_t)window->x,
                             (int32_t)(window->y + window->title_h - 1U),
                             (int32_t)window->w, 1,
                             cx0, cy0, cx1, cy1, border);
    }
    if (cx0 < (int32_t)(window->x + window->w) &&
        cx1 > (int32_t)(window->x + 2U) &&
        cy0 < (int32_t)(text_y + FONT_GLYPH_HEIGHT) &&
        cy1 > (int32_t)text_y) {
        font_draw_text_rect_clipped(fb, window->x + 2U, text_y,
                                    window->title_h, cx0, cy0, cx1, cy1,
                                    window->title, 0xfff0f4f8U);
    }

    min_bg = desktop != 0 && desktop->focused_window_id == index
                 ? 0xffa0a8b8U
                 : 0xff586070U;
    max_bg = min_bg;
    cls_bg = desktop != 0 && desktop->focused_window_id == index
                 ? 0xffe04a4aU
                 : 0xff8a3030U;

    if (gui_minimize_button_rect(window, &mb_x, &mb_y, &mb_w, &mb_h)) {
        gui_fillrect_clipped(fb, (int32_t)mb_x, (int32_t)mb_y,
                             (int32_t)mb_w, (int32_t)mb_h,
                             cx0, cy0, cx1, cy1, min_bg);
        gui_fillrect_clipped(fb, (int32_t)(mb_x + 2U),
                             (int32_t)(mb_y + mb_h - 3U),
                             (int32_t)(mb_w - 4U), 1,
                             cx0, cy0, cx1, cy1, 0xfff0f4f8U);
    }
    if (gui_maximize_button_rect(window, &xb_x, &xb_y, &xb_w, &xb_h)) {
        gui_fillrect_clipped(fb, (int32_t)xb_x, (int32_t)xb_y,
                             (int32_t)xb_w, (int32_t)xb_h,
                             cx0, cy0, cx1, cy1, max_bg);
        gui_draw_rect_clipped(fb, (int32_t)(xb_x + 2U),
                              (int32_t)(xb_y + 2U),
                              (int32_t)(xb_w - 4U),
                              (int32_t)(xb_h - 4U),
                              cx0, cy0, cx1, cy1, 0xfff0f4f8U);
    }
    if (gui_close_box_rect(window, &cb_x, &cb_y, &cb_w, &cb_h)) {
        gui_fillrect_clipped(fb, (int32_t)cb_x, (int32_t)cb_y,
                             (int32_t)cb_w, (int32_t)cb_h,
                             cx0, cy0, cx1, cy1, cls_bg);
        gui_draw_line_clipped(fb, (int32_t)(cb_x + 2U),
                              (int32_t)(cb_y + 2U),
                              (int32_t)(cb_x + cb_w - 3U),
                              (int32_t)(cb_y + cb_h - 3U),
                              cx0, cy0, cx1, cy1, 0xfff8f8f8U);
        gui_draw_line_clipped(fb, (int32_t)(cb_x + cb_w - 3U),
                              (int32_t)(cb_y + 2U),
                              (int32_t)(cb_x + 2U),
                              (int32_t)(cb_y + cb_h - 3U),
                              cx0, cy0, cx1, cy1, 0xfff8f8f8U);
    }
}

int gui_init(gui_desktop_t *desktop, fb_t *fb, uint32_t background_color) {
    if (desktop == 0 || fb == 0 || fb->pixels == 0) {
        return -1;
    }

    desktop->fb = fb;
    desktop->background_color = background_color;
    desktop->focused_window_id = GUI_NO_WINDOW;
    desktop->next_z = 1;
    desktop->drag_window_id = GUI_NO_WINDOW;
    desktop->drag_off_x = 0;
    desktop->drag_off_y = 0;
    desktop->cursor.x = (int32_t)(fb->width / 2U);
    desktop->cursor.y = (int32_t)(fb->height / 2U);
    desktop->cursor.prev_x = desktop->cursor.x;
    desktop->cursor.prev_y = desktop->cursor.y;
    desktop->cursor.buttons_mask = 0;
    desktop->cursor.visible = 1;
    desktop->cursor.shape = GUI_CURSOR_ARROW;
    desktop->damage_count = 0;
    desktop->damage_full = 1;

    for (uint32_t i = 0; i < GUI_MAX_WINDOWS; i++) {
        desktop->windows[i].x = 0;
        desktop->windows[i].y = 0;
        desktop->windows[i].w = 0;
        desktop->windows[i].h = 0;
        desktop->windows[i].bg_color = 0;
        desktop->windows[i].border_color = 0;
        desktop->windows[i].owner_pid = GUI_NO_OWNER;
        desktop->windows[i].z = 0;
        desktop->windows[i].flags = 0;
        desktop->windows[i].title_h = 0;
        for (uint32_t j = 0; j < GUI_TITLE_LEN; j++) {
            desktop->windows[i].title[j] = '\0';
        }
        desktop->windows[i].event_head = 0;
        desktop->windows[i].event_tail = 0;
        desktop->windows[i].event_count = 0;
        desktop->windows[i].used = 0;
        desktop->windows[i].owner_drawn = 0;
        desktop->windows[i].backing = 0;
        desktop->windows[i].backing_capacity = 0;
    }

    return 0;
}

int gui_window_draw_text(gui_desktop_t *desktop, uint32_t window_id,
                         int32_t x, int32_t y, const char *text,
                         uint32_t color) {
    gui_window_t *window;
    uint32_t content_h;
    uint32_t text_w;
    int32_t dw;
    int32_t dh;
    fb_t fb;

    if (desktop == 0 || window_id >= GUI_MAX_WINDOWS ||
        desktop->windows[window_id].used == 0 || text == 0) {
        return -1;
    }

    window = &desktop->windows[window_id];
    if (gui_window_ensure_backing(window) != 0) {
        return -1;
    }
    window->owner_drawn = 1;

    content_h = window->h > window->title_h ? window->h - window->title_h : 0U;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if ((uint32_t)x >= window->w || (uint32_t)y >= content_h) {
        return 0;
    }

    fb = gui_window_backing_fb(window);
    font_draw_text(&fb, (uint32_t)x, (uint32_t)y, text, color);
    text_w = font_text_width(text);
    dw = (int32_t)text_w;
    dh = (int32_t)FONT_GLYPH_HEIGHT;
    if (x + dw > (int32_t)window->w) {
        dw = (int32_t)window->w - x;
    }
    if (y + dh > (int32_t)content_h) {
        dh = (int32_t)content_h - y;
    }
    gui_damage_add(desktop, (int32_t)window->x + x,
                   (int32_t)window->y + (int32_t)window->title_h + y,
                   dw, dh);
    return 0;
}

int gui_window_draw_rect(gui_desktop_t *desktop, uint32_t window_id,
                         int32_t x, int32_t y, uint32_t w, uint32_t h,
                         uint32_t color) {
    gui_window_t *window;
    uint32_t content_h;
    int64_t x0;
    int64_t y0;
    int64_t x1;
    int64_t y1;
    fb_t fb;

    if (desktop == 0 || window_id >= GUI_MAX_WINDOWS ||
        desktop->windows[window_id].used == 0) {
        return -1;
    }
    if (w == 0U || h == 0U) {
        return 0;
    }

    window = &desktop->windows[window_id];
    if (gui_window_ensure_backing(window) != 0) {
        return -1;
    }
    window->owner_drawn = 1;

    content_h = window->h > window->title_h ? window->h - window->title_h : 0U;
    x0 = x;
    y0 = y;
    x1 = x0 + (int64_t)w;
    y1 = y0 + (int64_t)h;
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > (int64_t)window->w) {
        x1 = (int64_t)window->w;
    }
    if (y1 > (int64_t)content_h) {
        y1 = (int64_t)content_h;
    }
    if (x1 <= x0 || y1 <= y0) {
        return 0;
    }

    fb = gui_window_backing_fb(window);
    fb_fillrect(&fb, (uint32_t)x0, (uint32_t)y0, (uint32_t)(x1 - x0),
                (uint32_t)(y1 - y0), color);
    gui_damage_add(desktop, (int32_t)window->x + (int32_t)x0,
                   (int32_t)window->y + (int32_t)window->title_h +
                       (int32_t)y0,
                   (int32_t)(x1 - x0), (int32_t)(y1 - y0));
    return 0;
}

int gui_window_clear(gui_desktop_t *desktop, uint32_t window_id,
                     uint32_t color) {
    gui_window_t *window;
    uint32_t content_h;
    fb_t fb;

    if (desktop == 0 || window_id >= GUI_MAX_WINDOWS ||
        desktop->windows[window_id].used == 0) {
        return -1;
    }

    window = &desktop->windows[window_id];
    if (gui_window_ensure_backing(window) != 0) {
        return -1;
    }
    window->owner_drawn = 1;
    content_h = window->h > window->title_h ? window->h - window->title_h : 0U;
    fb = gui_window_backing_fb(window);
    fb_fillrect(&fb, 0, 0, window->w, content_h, color);
    gui_damage_add(desktop, (int32_t)window->x,
                   (int32_t)window->y + (int32_t)window->title_h,
                   (int32_t)window->w, (int32_t)content_h);
    return 0;
}

static void gui_draw_window(fb_t *fb, const gui_desktop_t *desktop,
                            uint32_t index, const gui_window_t *window) {
    uint32_t border;

    if (fb == 0 || window == 0 || window->used == 0) {
        return;
    }
    /* Minimised windows are completely off-screen. The panel greys the
     * matching running-apps slot and clicking it calls
     * gui_window_restore, which clears this flag and re-runs this
     * draw path with the window visible. */
    if (window->minimized != 0U) {
        return;
    }

    border = window->border_color;
    if (desktop != 0 && desktop->focused_window_id == index &&
        (window->flags & GUI_WINDOW_NO_FOCUS) == 0U) {
        border = 0xffe0e8f0U;
    }

    if (window->owner_drawn != 0 && window->backing != 0) {
        uint32_t content_h = window->h > window->title_h
                                 ? window->h - window->title_h
                                 : 0U;
        fb_blit(fb, window->x, window->y + window->title_h,
                window->backing, window->w, content_h);
    } else {
        fb_fillrect(fb, window->x, window->y, window->w, window->h, border);
        if (window->w > 2U && window->h > 2U) {
            fb_fillrect(fb, window->x + 1U, window->y + 1U,
                        window->w - 2U, window->h - 2U,
                        window->bg_color);
        }
    }

    fb_fillrect(fb, window->x, window->y, window->w, 1U, border);
    if (window->h > 1U) {
        fb_fillrect(fb, window->x, window->y + window->h - 1U,
                    window->w, 1U, border);
    }
    if (window->h > 2U) {
        fb_fillrect(fb, window->x, window->y + 1U, 1U,
                    window->h - 2U, border);
        if (window->w > 1U) {
            fb_fillrect(fb, window->x + window->w - 1U,
                        window->y + 1U, 1U, window->h - 2U, border);
        }
    }

    gui_draw_window_title(fb, desktop, index, window, border,
                          0, 0, (int32_t)fb->width, (int32_t)fb->height);
}

static void gui_draw_window_clipped(fb_t *fb, const gui_desktop_t *desktop,
                                    uint32_t index,
                                    const gui_window_t *window,
                                    int32_t cx0, int32_t cy0,
                                    int32_t cx1, int32_t cy1) {
    uint32_t border;

    if (fb == 0 || window == 0 || window->used == 0 ||
        window->minimized != 0U) {
        return;
    }
    if (!gui_rects_overlap((int32_t)window->x, (int32_t)window->y,
                           (int32_t)window->w, (int32_t)window->h,
                           cx0, cy0, cx1 - cx0, cy1 - cy0)) {
        return;
    }

    border = window->border_color;
    if (desktop != 0 && desktop->focused_window_id == index &&
        (window->flags & GUI_WINDOW_NO_FOCUS) == 0U) {
        border = 0xffe0e8f0U;
    }

    if (window->owner_drawn != 0 && window->backing != 0) {
        uint32_t content_h = window->h > window->title_h
                                 ? window->h - window->title_h
                                 : 0U;
        gui_blit_clipped(fb, (int32_t)window->x,
                         (int32_t)(window->y + window->title_h),
                         window->backing, window->w, content_h,
                         cx0, cy0, cx1, cy1);
    } else {
        gui_fillrect_clipped(fb, (int32_t)window->x, (int32_t)window->y,
                             (int32_t)window->w, (int32_t)window->h,
                             cx0, cy0, cx1, cy1, border);
        if (window->w > 2U && window->h > 2U) {
            gui_fillrect_clipped(fb, (int32_t)(window->x + 1U),
                                 (int32_t)(window->y + 1U),
                                 (int32_t)(window->w - 2U),
                                 (int32_t)(window->h - 2U),
                                 cx0, cy0, cx1, cy1, window->bg_color);
        }
    }

    gui_fillrect_clipped(fb, (int32_t)window->x, (int32_t)window->y,
                         (int32_t)window->w, 1, cx0, cy0, cx1, cy1, border);
    if (window->h > 1U) {
        gui_fillrect_clipped(fb, (int32_t)window->x,
                             (int32_t)(window->y + window->h - 1U),
                             (int32_t)window->w, 1,
                             cx0, cy0, cx1, cy1, border);
    }
    if (window->h > 2U) {
        gui_fillrect_clipped(fb, (int32_t)window->x,
                             (int32_t)(window->y + 1U), 1,
                             (int32_t)(window->h - 2U),
                             cx0, cy0, cx1, cy1, border);
        if (window->w > 1U) {
            gui_fillrect_clipped(fb, (int32_t)(window->x + window->w - 1U),
                                 (int32_t)(window->y + 1U), 1,
                                 (int32_t)(window->h - 2U),
                                 cx0, cy0, cx1, cy1, border);
        }
    }

    if (window->title_h > 0U &&
        cy0 < (int32_t)(window->y + window->title_h) &&
        cy1 > (int32_t)window->y) {
        gui_draw_window_title(fb, desktop, index, window, border,
                              cx0, cy0, cx1, cy1);
    }
}

/*
 * Find the next used window covering (x, row) whose left edge is at or
 * after x. Returns the window index or GUI_MAX_WINDOWS if none. Used by
 * the full-redraw path to skip window spans when painting the gradient.
 */
static uint32_t gui_next_window_at_or_after(const gui_desktop_t *desktop,
                                            uint32_t row, uint32_t x) {
    uint32_t best = GUI_MAX_WINDOWS;
    uint32_t best_x = UINT32_MAX;
    for (uint32_t i = 0; i < GUI_MAX_WINDOWS; i++) {
        const gui_window_t *w = &desktop->windows[i];
        if (w->used == 0 || w->minimized != 0U) {
            continue;
        }
        if (w->y > row || (w->y + w->h) <= row) {
            continue;
        }
        if (w->x + w->w <= x) {
            continue;
        }
        if (w->x >= x && w->x < best_x) {
            best = i;
            best_x = w->x;
        }
    }
    return best;
}

static uint32_t gui_next_window_above_z(const gui_desktop_t *desktop,
                                        uint32_t min_z) {
    uint32_t best = GUI_MAX_WINDOWS;
    uint32_t best_z = UINT32_MAX;

    for (uint32_t i = 0; i < GUI_MAX_WINDOWS; i++) {
        const gui_window_t *window = &desktop->windows[i];
        if (window->used == 0 || window->minimized != 0U ||
            window->z <= min_z) {
            continue;
        }
        if (window->z < best_z) {
            best = i;
            best_z = window->z;
        }
    }

    return best;
}

void gui_draw(gui_desktop_t *desktop) {
    if (desktop == 0 || desktop->fb == 0) {
        return;
    }

    /* "Full" sentinel: take the cheap route of repainting the entire
     * framebuffer. The damage list is ignored because the cost of
     * walking many small rects would exceed the cost of one full pass
     * once the bursts accumulate. */
    if (desktop->damage_full) {
        uint32_t bottom_color;
        uint32_t height;
        uint32_t row;
        uint32_t last_z;

        height = desktop->fb->height;
        bottom_color = gui_blend_color(desktop->background_color,
                                       0xff000000U, 3U, 4U);
        for (row = 0; row < height; row++) {
            uint32_t x = 0;
            while (x < desktop->fb->width) {
                uint32_t w = gui_next_window_at_or_after(desktop, row, x);
                uint32_t next;
                if (w == GUI_MAX_WINDOWS) {
                    next = desktop->fb->width;
                } else {
                    next = desktop->windows[w].x;
                }
                if (next > x) {
                    uint32_t color = gui_blend_color(
                        desktop->background_color, bottom_color, row,
                        height - 1U);
                    fb_fillrect(desktop->fb, x, row, next - x, 1U, color);
                }
                if (w == GUI_MAX_WINDOWS) {
                    break;
                }
                x = desktop->windows[w].x + desktop->windows[w].w;
            }
        }
        last_z = 0;
        for (;;) {
            uint32_t i = gui_next_window_above_z(desktop, last_z);
            if (i == GUI_MAX_WINDOWS) {
                break;
            }
            gui_draw_window(desktop->fb, desktop, i, &desktop->windows[i]);
            last_z = desktop->windows[i].z;
        }
        if (desktop->cursor.visible) {
            gui_draw_cursor(desktop->fb, desktop->cursor.x,
                            desktop->cursor.y, desktop->cursor.shape);
        }
        return;
    }

    /* Partial-redraw path: walk the damage list and repaint each rect.
     * The gradient is painted without skipping window spans (it is
     * cheap; one fillrect per row) and the windows are then repainted
     * in z-order, which overdraws where they overlap. The visual
     * result matches the full path; the per-row cost is proportional
     * to the damage area instead of the framebuffer size. */
    for (uint32_t i = 0; i < desktop->damage_count; i++) {
        const damage_rect_t *r = &desktop->damage_rects[i];
        int32_t x0 = r->x;
        int32_t y0 = r->y;
        int32_t x1 = r->x + r->w;
        int32_t y1 = r->y + r->h;
        if (x0 < 0) {
            x0 = 0;
        }
        if (y0 < 0) {
            y0 = 0;
        }
        if (x1 > (int32_t)desktop->fb->width) {
            x1 = (int32_t)desktop->fb->width;
        }
        if (y1 > (int32_t)desktop->fb->height) {
            y1 = (int32_t)desktop->fb->height;
        }
        if (x1 <= x0 || y1 <= y0) {
            continue;
        }
        uint32_t height = desktop->fb->height;
        uint32_t bottom_color = gui_blend_color(desktop->background_color,
                                                0xff000000U, 3U, 4U);
        for (int32_t row = y0; row < y1; row++) {
            uint32_t color = gui_blend_color(desktop->background_color,
                                             bottom_color, (uint32_t)row,
                                             height - 1U);
            fb_fillrect(desktop->fb, (uint32_t)x0, (uint32_t)row,
                        (uint32_t)(x1 - x0), 1U, color);
        }
        uint32_t last_z = 0;
        for (;;) {
            uint32_t wi = gui_next_window_above_z(desktop, last_z);
            if (wi == GUI_MAX_WINDOWS) {
                break;
            }
            const gui_window_t *win = &desktop->windows[wi];
            /* Skip windows whose bounding box doesn't overlap this
             * damage rect — avoids unnecessary redraws. */
            if ((int32_t)(win->x + win->w) > x0 &&
                (int32_t)win->x < x1 &&
                (int32_t)(win->y + win->h) > y0 &&
                (int32_t)win->y < y1) {
                gui_draw_window_clipped(desktop->fb, desktop, wi, win,
                                        x0, y0, x1, y1);
            }
            last_z = win->z;
        }
        if (desktop->cursor.visible) {
            int32_t cx0 = desktop->cursor.x;
            int32_t cy0 = desktop->cursor.y;
            int32_t cx1 = cx0 + (int32_t)GUI_CURSOR_W;
            int32_t cy1 = cy0 + (int32_t)GUI_CURSOR_H;
            if (cx1 > x0 && cx0 < x1 && cy1 > y0 && cy0 < y1) {
                gui_draw_cursor(desktop->fb, desktop->cursor.x,
                                desktop->cursor.y, desktop->cursor.shape);
            }
        }
    }
}

void gui_render(fb_t *fb, void *context) {
    (void)context;
    if (g_gui_active == 0) {
        return;
    }
    g_gui_fb = *fb;
    g_gui_desktop.fb = &g_gui_fb;
    gui_draw(&g_gui_desktop);
}

gui_desktop_t *gui_desktop(void) {
    if (g_gui_active == 0) {
        return 0;
    }
    return &g_gui_desktop;
}

int gui_is_dirty(void) {
    return g_gui_dirty != 0U ? 1 : 0;
}

void gui_clear_dirty(void) {
    g_gui_dirty = 0;
    if (g_gui_active != 0U) {
        gui_damage_clear(&g_gui_desktop);
    }
}

void gui_request_redraw(void) {
    g_gui_dirty = 1;
    if (g_gui_active != 0U) {
        gui_damage_add_full(&g_gui_desktop);
    }
}

void gui_mark_dirty(void) {
    g_gui_dirty = 1;
}

int gui_handle_input(const input_event_t *event) {
    if (g_gui_active == 0 || event == 0) {
        return -1;
    }
    return gui_handle_desktop_input(&g_gui_desktop, event);
}

void gui_init_for_framebuffer(fb_t *fb, void *context) {
    (void)context;
    g_gui_active = 0;
    g_gui_dirty = 0;

    if (fb == 0) {
        return;
    }

    g_gui_fb = *fb;
    if (gui_init(&g_gui_desktop, &g_gui_fb, 0xff202428U) != 0) {
        return;
    }
    gui_draw(&g_gui_desktop);
    gui_damage_clear(&g_gui_desktop);
    g_gui_active = 1;
}
