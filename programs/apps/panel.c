// ArmoniOS app: panel (C version, on libkarm + libkarmdesk)
//
// Keep the popup in the same KLI1 translation unit as the taskbar. The
// panel-menu receives a one-pixel kernel title area: this preserves normal
// keyboard focus while preventing clicks in its content from starting a drag.

#include "libkarmdesk/gui.h"

static int panel_is_menu_title(const char *title) {
    static const char menu_title[] = "panel-menu";
    int i = 0;

    if (title == 0) {
        return 0;
    }
    while (menu_title[i] != '\0') {
        if (title[i] != menu_title[i]) {
            return 0;
        }
        i++;
    }
    return title[i] == '\0';
}

static long panel_window_create(long x, long y, long w, long h,
                                long bg, long border, const char *title) {
    long window_id = gui_window_create(x, y, w, h, bg, border, title);

    if (window_id >= 0 && panel_is_menu_title(title)) {
        if (gui_window_set_title(window_id, title, 1) < 0) {
            (void)gui_window_destroy(window_id);
            return -1;
        }
    }
    return window_id;
}

#define gui_window_create panel_window_create
#include "panel_runtime.inc"
