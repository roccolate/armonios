#ifndef ARMONIOS_PROGRAMS_LIBARMDESK_THEME_H
#define ARMONIOS_PROGRAMS_LIBARMDESK_THEME_H

#include <stdint.h>

/*
 * Semantic color tokens aligned with retrocore-spec/contracts/theme-tokens.md.
 *
 * Values are ArmoniOS framebuffer colors in 0xAARRGGBB form. The token names
 * are stable UI vocabulary; individual themes may choose different values.
 */

typedef enum {
    ARMDESK_THEME_DESKTOP_BG = 0,
    ARMDESK_THEME_WINDOW_BG,
    ARMDESK_THEME_WINDOW_FG,
    ARMDESK_THEME_WINDOW_BORDER,
    ARMDESK_THEME_WINDOW_TITLE_BG,
    ARMDESK_THEME_WINDOW_TITLE_FG,
    ARMDESK_THEME_BUTTON_BG,
    ARMDESK_THEME_BUTTON_FG,
    ARMDESK_THEME_SELECTION_BG,
    ARMDESK_THEME_SELECTION_FG,
    ARMDESK_THEME_STATUS_BG,
    ARMDESK_THEME_STATUS_FG,
    ARMDESK_THEME_WARNING_FG,
    ARMDESK_THEME_ERROR_FG,
    ARMDESK_THEME_MENU_BG,
    ARMDESK_THEME_MENU_FG,
    ARMDESK_THEME_MENU_HOTKEY_FG,
    ARMDESK_THEME_PANEL_BG,
    ARMDESK_THEME_PANEL_FG,
    ARMDESK_THEME_ICON_FG,
    ARMDESK_THEME_SHADOW_BG,
    ARMDESK_THEME_DISABLED_FG,
    ARMDESK_THEME_TOKEN_COUNT
} armdesk_theme_token_t;

typedef struct {
    uint32_t colors[ARMDESK_THEME_TOKEN_COUNT];
} armdesk_theme_t;

/*
 * Header-only initializer: no bytes are added to an application unless it
 * explicitly instantiates and uses a theme.
 */
#define ARMDESK_THEME_DARK_INIT { \
    .colors = { \
        [ARMDESK_THEME_DESKTOP_BG]      = 0xff10181cU, \
        [ARMDESK_THEME_WINDOW_BG]       = 0xff18242cU, \
        [ARMDESK_THEME_WINDOW_FG]       = 0xffdce8e8U, \
        [ARMDESK_THEME_WINDOW_BORDER]   = 0xff789090U, \
        [ARMDESK_THEME_WINDOW_TITLE_BG] = 0xff243640U, \
        [ARMDESK_THEME_WINDOW_TITLE_FG] = 0xfff0f4f4U, \
        [ARMDESK_THEME_BUTTON_BG]       = 0xff304650U, \
        [ARMDESK_THEME_BUTTON_FG]       = 0xffe4eeeeU, \
        [ARMDESK_THEME_SELECTION_BG]    = 0xff385068U, \
        [ARMDESK_THEME_SELECTION_FG]    = 0xffffffffU, \
        [ARMDESK_THEME_STATUS_BG]       = 0xff142026U, \
        [ARMDESK_THEME_STATUS_FG]       = 0xffb8e0c0U, \
        [ARMDESK_THEME_WARNING_FG]      = 0xffe0b8a0U, \
        [ARMDESK_THEME_ERROR_FG]        = 0xffff9090U, \
        [ARMDESK_THEME_MENU_BG]         = 0xff202f36U, \
        [ARMDESK_THEME_MENU_FG]         = 0xffdce8e8U, \
        [ARMDESK_THEME_MENU_HOTKEY_FG]  = 0xffe0c080U, \
        [ARMDESK_THEME_PANEL_BG]        = 0xff142026U, \
        [ARMDESK_THEME_PANEL_FG]        = 0xffdce8e8U, \
        [ARMDESK_THEME_ICON_FG]         = 0xffdce8e8U, \
        [ARMDESK_THEME_SHADOW_BG]       = 0xff080c0eU, \
        [ARMDESK_THEME_DISABLED_FG]     = 0xff78888cU, \
    } \
}

static inline uint32_t armdesk_theme_color(const armdesk_theme_t *theme,
                                            armdesk_theme_token_t token) {
    if (theme == 0 ||
        (unsigned)token >= (unsigned)ARMDESK_THEME_TOKEN_COUNT) {
        return 0U;
    }
    return theme->colors[token];
}

#endif
