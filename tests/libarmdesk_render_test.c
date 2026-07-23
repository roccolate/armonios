#include <stdint.h>

#include "programs/libarmdesk/render.h"

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static long g_call5[6];
static long g_call6[7];
static int g_call5_count;
static int g_call6_count;

long __syscall5(long n, long a0, long a1, long a2, long a3, long a4) {
    g_call5[0] = n;
    g_call5[1] = a0;
    g_call5[2] = a1;
    g_call5[3] = a2;
    g_call5[4] = a3;
    g_call5[5] = a4;
    g_call5_count++;
    return 0;
}

long __syscall6(long n, long a0, long a1, long a2, long a3, long a4,
                long a5) {
    g_call6[0] = n;
    g_call6[1] = a0;
    g_call6[2] = a1;
    g_call6[3] = a2;
    g_call6[4] = a3;
    g_call6[5] = a4;
    g_call6[6] = a5;
    g_call6_count++;
    return 0;
}

static int test_label_render(void) {
    const armdesk_theme_t theme = ARMDESK_THEME_DARK_INIT;
    armdesk_label_t label = armdesk_label(
        armdesk_rect(12, 8, 100, 12), "Status", ARMDESK_THEME_STATUS_FG);

    g_call5_count = 0;
    CHECK(armdesk_render_label(7, &label, &theme) == 0);
    CHECK(g_call5_count == 1);
    CHECK(g_call5[0] == SYS_WINDOW_DRAW_TEXT);
    CHECK(g_call5[1] == 7);
    CHECK(g_call5[2] == 12);
    CHECK(g_call5[3] == 8);
    CHECK((uint32_t)g_call5[4] ==
          armdesk_theme_color(&theme, ARMDESK_THEME_STATUS_FG));
    CHECK((const char *)(uintptr_t)g_call5[5] == label.text);
    return 0;
}

static int test_button_render(void) {
    const armdesk_theme_t theme = ARMDESK_THEME_DARK_INIT;
    armdesk_button_t button =
        armdesk_button(armdesk_rect(20, 30, 60, 18), "Save");

    g_call5_count = 0;
    g_call6_count = 0;
    CHECK(armdesk_render_button(9, &button, &theme) == 0);
    CHECK(g_call6_count == 2);
    CHECK(g_call5_count == 1);
    CHECK(g_call6[0] == SYS_WINDOW_DRAW_RECT);
    CHECK(g_call6[1] == 9);
    CHECK(g_call6[2] == 21);
    CHECK(g_call6[3] == 31);
    CHECK(g_call6[4] == 58);
    CHECK(g_call6[5] == 16);
    CHECK((uint32_t)g_call6[6] ==
          armdesk_theme_color(&theme, ARMDESK_THEME_BUTTON_BG));
    CHECK(g_call5[2] == 26);
    CHECK(g_call5[3] == 35);
    CHECK((uint32_t)g_call5[4] ==
          armdesk_theme_color(&theme, ARMDESK_THEME_BUTTON_FG));

    armdesk_button_set_enabled(&button, 0);
    CHECK(armdesk_render_button(9, &button, &theme) == 0);
    CHECK((uint32_t)g_call5[4] ==
          armdesk_theme_color(&theme, ARMDESK_THEME_DISABLED_FG));
    return 0;
}

static int test_render_rejection(void) {
    const armdesk_theme_t theme = ARMDESK_THEME_DARK_INIT;
    armdesk_label_t label = armdesk_label(
        armdesk_rect(0, 0, 0, 10), "Empty", ARMDESK_THEME_WINDOW_FG);
    armdesk_button_t button =
        armdesk_button(armdesk_rect(0, 0, 2, 2), "Tiny");
    int calls5 = g_call5_count;
    int calls6 = g_call6_count;

    CHECK(armdesk_render_label(1, &label, &theme) != 0);
    CHECK(armdesk_render_button(1, &button, &theme) != 0);
    CHECK(g_call5_count == calls5);
    CHECK(g_call6_count == calls6);
    return 0;
}

int main(void) {
    int result = test_label_render();
    if (result != 0) return result;
    result = test_button_render();
    if (result != 0) return result;
    return test_render_rejection();
}
