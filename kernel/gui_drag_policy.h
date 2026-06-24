#ifndef KOLIBRIARM_GUI_DRAG_POLICY_H
#define KOLIBRIARM_GUI_DRAG_POLICY_H

/*
 * gui.c historically exposes gui_drag_start as a normal function and calls it
 * from gui_handle_input. Mark that definition weak so gui_drag_policy.c can
 * provide the window-manager policy without rewriting the rest of gui.c.
 */
#pragma weak gui_drag_start

#endif
