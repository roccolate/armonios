#include <stdint.h>

#include "include/armonios/abi/gui.h"
#include "programs/libarmdesk/rect.h"
#include "programs/libarmdesk/theme.h"

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int test_gui_abi(void) {
    CHECK(sizeof(gui_event_t) == 12U);
    CHECK(GUI_EVENT_KEY_PRESS == 1U);
    CHECK(GUI_EVENT_MAXIMIZE == 8U);
    CHECK(GUI_WINDOW_STATE_MINIMIZED == 1U);
    CHECK(GUI_WINDOW_STATE_FOCUSED == 2U);
    CHECK(GUI_CURSOR_ARROW == 0U);
    CHECK(GUI_CURSOR_HAND == 1U);
    CHECK(GUI_CURSOR_REGION_DELETE == 0xffffffffU);
    return 0;
}

static int test_rect_helpers(void) {
    armdesk_rect_t a = armdesk_rect(10, 10, 20, 20);
    armdesk_rect_t b = armdesk_rect(25, 5, 20, 20);
    armdesk_rect_t overlap = armdesk_rect_intersection(a, b);
    armdesk_rect_t clipped = armdesk_rect_clamp_to_bounds(
        armdesk_rect(-5, -4, 12, 10), 100, 100);

    CHECK(!armdesk_rect_is_empty(a));
    CHECK(armdesk_rect_contains(a, 10, 10));
    CHECK(armdesk_rect_contains(a, 29, 29));
    CHECK(!armdesk_rect_contains(a, 30, 29));
    CHECK(armdesk_rect_intersects(a, b));
    CHECK(overlap.x == 25);
    CHECK(overlap.y == 10);
    CHECK(overlap.w == 5);
    CHECK(overlap.h == 15);
    CHECK(clipped.x == 0);
    CHECK(clipped.y == 0);
    CHECK(clipped.w == 7);
    CHECK(clipped.h == 6);
    CHECK(!armdesk_rect_intersects(a, armdesk_rect(30, 10, 5, 5)));
    return 0;
}

static int test_theme_tokens(void) {
    const armdesk_theme_t theme = ARMDESK_THEME_DARK_INIT;

    CHECK(ARMDESK_THEME_TOKEN_COUNT == 22);
    CHECK(armdesk_theme_color(&theme, ARMDESK_THEME_WINDOW_BG) ==
          0xff18242cU);
    CHECK(armdesk_theme_color(&theme, ARMDESK_THEME_SELECTION_BG) ==
          0xff385068U);
    CHECK(armdesk_theme_color(&theme, ARMDESK_THEME_WARNING_FG) ==
          0xffe0b8a0U);
    CHECK(armdesk_theme_color(0, ARMDESK_THEME_WINDOW_BG) == 0U);
    CHECK(armdesk_theme_color(&theme,
          (armdesk_theme_token_t)ARMDESK_THEME_TOKEN_COUNT) == 0U);
    return 0;
}

int main(void) {
    int result = test_gui_abi();
    if (result != 0) return result;
    result = test_rect_helpers();
    if (result != 0) return result;
    return test_theme_tokens();
}
