#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "programs/apps/panel_model.h"

static int failures;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL:%d: %s\n", __LINE__, #expr); \
        failures++; \
    } \
} while (0)

int main(void) {
    uint32_t states[PANEL_APP_WINDOW_CAP] = {0};
    char clock_text[9];

    CHECK(panel_target_at(PANEL_MENU_BUTTON_X, PANEL_ITEM_Y) ==
          PANEL_TARGET_MENU);
    CHECK(panel_target_at(PANEL_MENU_BUTTON_X + PANEL_MENU_BUTTON_W - 1,
                          PANEL_ITEM_Y + PANEL_ITEM_H - 1) ==
          PANEL_TARGET_MENU);
    CHECK(panel_target_at(panel_app_x(0), PANEL_ITEM_Y) == 0);
    CHECK(panel_target_at(panel_app_x(3) + PANEL_APP_W - 1,
                          PANEL_ITEM_Y + PANEL_ITEM_H - 1) == 3);
    CHECK(panel_target_at(panel_app_x(0) + PANEL_APP_W,
                          PANEL_ITEM_Y + 1) == PANEL_TARGET_NONE);
    CHECK(PANEL_APP_W == 36);
    CHECK(PANEL_MENU_BUTTON_W == 56);
    CHECK(PANEL_APP_X + PANEL_PINNED_COUNT * PANEL_APP_W +
              (PANEL_PINNED_COUNT - 1) * PANEL_APP_GAP <= PANEL_CLOCK_X);
    CHECK(panel_target_at(PANEL_CLOCK_X, PANEL_ITEM_Y) == PANEL_TARGET_CLOCK);
    CHECK(panel_target_at(PANEL_CLOCK_X + PANEL_CLOCK_W,
                          PANEL_ITEM_Y) == PANEL_TARGET_NONE);
    CHECK(panel_target_at(10, PANEL_ITEM_Y - 1) == PANEL_TARGET_NONE);

    for (int i = 0; i < PANEL_APP_COUNT; i++) {
        CHECK(panel_menu_item_at(PANEL_MENU_ITEM_X,
                                 panel_menu_item_y(i)) == i);
        CHECK(panel_menu_item_at(PANEL_MENU_ITEM_X + PANEL_MENU_ITEM_W - 1,
                                 panel_menu_item_y(i) +
                                     PANEL_MENU_ITEM_H - 1) == i);
    }
    CHECK(panel_menu_item_at(PANEL_MENU_ITEM_X - 1,
                             panel_menu_item_y(0)) == -1);
    CHECK(panel_menu_item_at(PANEL_MENU_ITEM_X,
                             panel_menu_item_y(0) +
                                 PANEL_MENU_ITEM_H) == -1);

    CHECK(panel_visual_state(states, 0) == PANEL_VISUAL_CLOSED);
    CHECK(panel_app_visual_state(states, 0, 0) == PANEL_VISUAL_CLOSED);
    CHECK(panel_app_visual_state(states, 0, 1) == PANEL_VISUAL_RUNNING);
    states[0] = 0;
    CHECK(panel_visual_state(states, 1) == PANEL_VISUAL_RUNNING);
    states[0] = PANEL_WINDOW_STATE_MINIMIZED;
    CHECK(panel_visual_state(states, 1) == PANEL_VISUAL_MINIMIZED);
    states[0] = PANEL_WINDOW_STATE_FOCUSED;
    CHECK(panel_visual_state(states, 1) == PANEL_VISUAL_FOCUSED);
    states[0] = PANEL_WINDOW_STATE_MINIMIZED;
    states[1] = 0;
    CHECK(panel_visual_state(states, 2) == PANEL_VISUAL_RUNNING);
    states[1] = PANEL_WINDOW_STATE_FOCUSED;
    CHECK(panel_visual_state(states, 2) == PANEL_VISUAL_FOCUSED);

    CHECK(panel_pick_window(states, 0) == -1);
    states[0] = 0;
    states[1] = PANEL_WINDOW_STATE_MINIMIZED;
    states[2] = PANEL_WINDOW_STATE_MINIMIZED;
    CHECK(panel_pick_window(states, 3) == 0);
    states[0] = PANEL_WINDOW_STATE_MINIMIZED;
    CHECK(panel_pick_window(states, 3) == 0);
    states[1] = PANEL_WINDOW_STATE_FOCUSED;
    CHECK(panel_pick_window(states, 3) == 2);
    states[1] = 0;
    states[2] = PANEL_WINDOW_STATE_FOCUSED;
    CHECK(panel_pick_window(states, 3) == 0);

    CHECK(panel_activation_action(states, 0, 0) == PANEL_ACTION_LAUNCH);
    CHECK(panel_activation_action(states, 0, 1) == PANEL_ACTION_WAIT);
    states[0] = 0;
    CHECK(panel_activation_action(states, 1, 1) == PANEL_ACTION_FOCUS);
    states[0] = PANEL_WINDOW_STATE_MINIMIZED;
    CHECK(panel_activation_action(states, 1, 1) == PANEL_ACTION_RESTORE);
    states[0] = PANEL_WINDOW_STATE_FOCUSED;
    CHECK(panel_activation_action(states, 1, 1) == PANEL_ACTION_NONE);
    states[0] = PANEL_WINDOW_STATE_FOCUSED;
    states[1] = 0;
    CHECK(panel_activation_action(states, 2, 2) == PANEL_ACTION_FOCUS);
    CHECK(panel_pick_window(states, 2) == 1);
    states[1] = PANEL_WINDOW_STATE_MINIMIZED;
    CHECK(panel_activation_action(states, 2, 2) == PANEL_ACTION_RESTORE);

    panel_format_uptime(0, clock_text);
    CHECK(strcmp(clock_text, "00:00:00") == 0);
    panel_format_uptime(3661ULL * PANEL_TICKS_PER_SECOND, clock_text);
    CHECK(strcmp(clock_text, "01:01:01") == 0);
    panel_format_uptime((100ULL * 3600ULL + 2ULL) * PANEL_TICKS_PER_SECOND,
                        clock_text);
    CHECK(strcmp(clock_text, "00:00:02") == 0);

    if (failures != 0) {
        return 1;
    }
    puts("panel-model-test: PASS");
    return 0;
}
