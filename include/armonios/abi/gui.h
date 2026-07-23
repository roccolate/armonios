#ifndef ARMONIOS_INCLUDE_ARMONIOS_ABI_GUI_H
#define ARMONIOS_INCLUDE_ARMONIOS_ABI_GUI_H

#include <stdint.h>

/*
 * Public GUI ABI shared by the kernel and userland.
 *
 * Keep this header limited to values and layouts that cross the syscall
 * boundary. Compositor internals, window storage, input drivers, and drawing
 * policy remain kernel-private.
 */

#define GUI_EVENT_KEY_PRESS   1U
#define GUI_EVENT_KEY_RELEASE 2U
#define GUI_EVENT_MOUSE_CLICK 3U
#define GUI_EVENT_MOUSE_MOVE  4U
#define GUI_EVENT_RESIZE      5U
#define GUI_EVENT_CLOSE       6U
#define GUI_EVENT_MINIMIZE    7U
#define GUI_EVENT_MAXIMIZE    8U

typedef struct {
    uint32_t type;
    int32_t  data1;
    int32_t  data2;
} gui_event_t;

_Static_assert(sizeof(gui_event_t) == 12,
               "ABI drift: gui_event_t must remain 12 bytes");

#define GUI_WINDOW_STATE_MINIMIZED (1U << 0)
#define GUI_WINDOW_STATE_FOCUSED   (1U << 1)

#define GUI_CURSOR_ARROW 0U
#define GUI_CURSOR_HAND  1U

#define GUI_CURSOR_REGION_DELETE 0xffffffffU

#define GUI_BUTTON_LEFT   (1U << 0)
#define GUI_BUTTON_RIGHT  (1U << 1)
#define GUI_BUTTON_MIDDLE (1U << 2)

#endif
