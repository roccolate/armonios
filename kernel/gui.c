#include "kernel/gui.h"

#include <stdint.h>

#include "kernel/font.h"

int gui_init(gui_desktop_t *desktop, fb_t *fb, uint32_t background_color) {
    if (desktop == 0 || fb == 0 || fb->pixels == 0) {
        return -1;
    }

    desktop->fb = fb;
    desktop->background_color = background_color;
    desktop->focused_window_id = GUI_NO_WINDOW;
    for (uint32_t i = 0; i < GUI_MAX_WINDOWS; i++) {
        desktop->windows[i].x = 0;
        desktop->windows[i].y = 0;
        desktop->windows[i].w = 0;
        desktop->windows[i].h = 0;
        desktop->windows[i].bg_color = 0;
        desktop->windows[i].border_color = 0;
        desktop->windows[i].key_count = 0;
        desktop->windows[i].last_key = '\0';
        desktop->windows[i].used = 0;
    }

    return 0;
}

int gui_create_window(gui_desktop_t *desktop, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, uint32_t bg_color,
                      uint32_t border_color, uint32_t *window_id) {
    if (desktop == 0 || desktop->fb == 0 || w < 2U || h < 2U ||
        x >= desktop->fb->width || y >= desktop->fb->height) {
        return -1;
    }

    for (uint32_t i = 0; i < GUI_MAX_WINDOWS; i++) {
        gui_window_t *window = &desktop->windows[i];

        if (window->used == 0) {
            window->x = x;
            window->y = y;
            window->w = w;
            window->h = h;
            window->bg_color = bg_color;
            window->border_color = border_color;
            window->key_count = 0;
            window->last_key = '\0';
            window->used = 1;
            if (desktop->focused_window_id == GUI_NO_WINDOW) {
                desktop->focused_window_id = i;
            }
            if (window_id != 0) {
                *window_id = i;
            }
            return 0;
        }
    }

    return -1;
}

int gui_move_window(gui_desktop_t *desktop, uint32_t window_id, uint32_t x,
                    uint32_t y) {
    gui_window_t *window;

    if (desktop == 0 || desktop->fb == 0 || window_id >= GUI_MAX_WINDOWS ||
        x >= desktop->fb->width || y >= desktop->fb->height ||
        desktop->windows[window_id].used == 0) {
        return -1;
    }

    window = &desktop->windows[window_id];
    window->x = x;
    window->y = y;
    return 0;
}

int gui_focus_window(gui_desktop_t *desktop, uint32_t window_id) {
    if (desktop == 0 || window_id >= GUI_MAX_WINDOWS ||
        desktop->windows[window_id].used == 0) {
        return -1;
    }

    desktop->focused_window_id = window_id;
    return 0;
}

int gui_send_key(gui_desktop_t *desktop, char key) {
    gui_window_t *window;

    if (desktop == 0 || desktop->focused_window_id >= GUI_MAX_WINDOWS ||
        desktop->windows[desktop->focused_window_id].used == 0) {
        return -1;
    }

    window = &desktop->windows[desktop->focused_window_id];
    window->last_key = key;
    window->key_count++;
    return 0;
}

static void gui_draw_window(fb_t *fb, const gui_window_t *window) {
    if (fb == 0 || window == 0 || window->used == 0) {
        return;
    }

    fb_fillrect(fb, window->x, window->y, window->w, window->h,
                window->border_color);
    if (window->w > 2U && window->h > 2U) {
        fb_fillrect(fb, window->x + 1U, window->y + 1U,
                    window->w - 2U, window->h - 2U, window->bg_color);
    }
}

void gui_draw(gui_desktop_t *desktop) {
    if (desktop == 0 || desktop->fb == 0) {
        return;
    }

    fb_fillrect(desktop->fb, 0, 0, desktop->fb->width, desktop->fb->height,
                desktop->background_color);
    for (uint32_t i = 0; i < GUI_MAX_WINDOWS; i++) {
        gui_draw_window(desktop->fb, &desktop->windows[i]);
    }
}

static fb_t g_demo_fb;
static gui_desktop_t g_demo_desktop;
static uint8_t g_demo_active;

void gui_draw_demo(fb_t *fb, void *context) {
    uint32_t first = GUI_NO_WINDOW;
    uint32_t second = GUI_NO_WINDOW;

    (void)context;
    g_demo_active = 0;

    if (fb == 0) {
        return;
    }

    g_demo_fb = *fb;
    if (gui_init(&g_demo_desktop, &g_demo_fb, 0xff202428U) != 0) {
        return;
    }

    (void)gui_create_window(&g_demo_desktop, 72, 64, 320, 220, 0xff2f6fedU,
                            0xffd8e4ffU, &first);
    (void)gui_create_window(&g_demo_desktop, 220, 150, 340, 230, 0xff38a169U,
                            0xfffff4c2U, &second);
    (void)gui_focus_window(&g_demo_desktop, second);
    gui_draw(&g_demo_desktop);
    font_draw_text(fb, 94, 86, "KOLIBRI ARM", 0xffffffffU);
    font_draw_text(fb, 242, 172, "FAT32 IPC", 0xff101010U);
    (void)first;
    g_demo_active = 1;
}

int gui_demo_send_key(char key) {
    if (g_demo_active == 0) {
        return -1;
    }

    return gui_send_key(&g_demo_desktop, key);
}
