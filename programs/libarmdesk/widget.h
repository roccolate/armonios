#ifndef ARMONIOS_PROGRAMS_LIBARMDESK_WIDGET_H
#define ARMONIOS_PROGRAMS_LIBARMDESK_WIDGET_H

#include <stdint.h>

#include "rect.h"
#include "theme.h"

typedef struct {
    armdesk_rect_t bounds;
    const char *text;
    armdesk_theme_token_t color;
} armdesk_label_t;

typedef struct {
    armdesk_rect_t bounds;
    const char *text;
    uint8_t enabled;
    uint8_t hovered;
    uint8_t pressed;
} armdesk_button_t;

static inline armdesk_label_t armdesk_label(
    armdesk_rect_t bounds, const char *text,
    armdesk_theme_token_t color) {
    armdesk_label_t label;

    label.bounds = bounds;
    label.text = text;
    label.color = color;
    return label;
}

static inline armdesk_button_t armdesk_button(armdesk_rect_t bounds,
                                               const char *text) {
    armdesk_button_t button;

    button.bounds = bounds;
    button.text = text;
    button.enabled = 1U;
    button.hovered = 0U;
    button.pressed = 0U;
    return button;
}

static inline void armdesk_button_set_enabled(armdesk_button_t *button,
                                               int enabled) {
    if (button == 0) {
        return;
    }

    button->enabled = enabled ? 1U : 0U;
    if (button->enabled == 0U) {
        button->hovered = 0U;
        button->pressed = 0U;
    }
}

static inline int armdesk_button_hit_test(const armdesk_button_t *button,
                                           int32_t x, int32_t y) {
    return button != 0 && button->enabled != 0U &&
           armdesk_rect_contains(button->bounds, x, y);
}

#endif