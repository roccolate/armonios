#ifndef ARMONIOS_PROGRAMS_LIBARMDESK_RENDER_H
#define ARMONIOS_PROGRAMS_LIBARMDESK_RENDER_H

#include <stdint.h>

#include "gui.h"
#include "theme.h"
#include "widget.h"

static inline int armdesk_render_fill(long window_id, armdesk_rect_t bounds,
                                      uint32_t color) {
    if (armdesk_rect_is_empty(bounds)) {
        return -1;
    }

    return gui_window_draw_rect(window_id, bounds.x, bounds.y,
                                bounds.w, bounds.h, color) < 0 ? -1 : 0;
}

static inline int armdesk_render_label(long window_id,
                                       const armdesk_label_t *label,
                                       const armdesk_theme_t *theme) {
    uint32_t color;

    if (label == 0 || label->text == 0 || theme == 0 ||
        armdesk_rect_is_empty(label->bounds)) {
        return -1;
    }

    color = armdesk_theme_color(theme, label->color);
    return gui_window_draw_text(window_id, label->bounds.x, label->bounds.y,
                                color, label->text) < 0 ? -1 : 0;
}

static inline int armdesk_render_button(long window_id,
                                        const armdesk_button_t *button,
                                        const armdesk_theme_t *theme) {
    armdesk_rect_t inner;
    int32_t text_y;
    uint32_t background;
    uint32_t foreground;
    uint32_t border;

    if (button == 0 || button->text == 0 || theme == 0 ||
        button->bounds.w < 3 || button->bounds.h < 3) {
        return -1;
    }

    border = armdesk_theme_color(theme, ARMDESK_THEME_WINDOW_BORDER);
    if (button->pressed != 0U) {
        background = armdesk_theme_color(theme, ARMDESK_THEME_SELECTION_BG);
    } else if (button->hovered != 0U) {
        background = armdesk_theme_color(theme, ARMDESK_THEME_MENU_BG);
    } else {
        background = armdesk_theme_color(theme, ARMDESK_THEME_BUTTON_BG);
    }
    foreground = armdesk_theme_color(
        theme, button->enabled != 0U ? ARMDESK_THEME_BUTTON_FG
                                    : ARMDESK_THEME_DISABLED_FG);

    if (armdesk_render_fill(window_id, button->bounds, border) != 0) {
        return -1;
    }

    inner = armdesk_rect(button->bounds.x + 1, button->bounds.y + 1,
                         button->bounds.w - 2, button->bounds.h - 2);
    if (armdesk_render_fill(window_id, inner, background) != 0) {
        return -1;
    }

    text_y = button->bounds.y;
    if (button->bounds.h > 8) {
        text_y += (button->bounds.h - 8) / 2;
    }
    return gui_window_draw_text(window_id, button->bounds.x + 6, text_y,
                                foreground, button->text) < 0 ? -1 : 0;
}

#endif