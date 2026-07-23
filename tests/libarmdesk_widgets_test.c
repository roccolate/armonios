#include <stdint.h>

#include "programs/libarmdesk/event.h"
#include "programs/libarmdesk/layout.h"
#include "programs/libarmdesk/widget.h"

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

static int rect_equal(armdesk_rect_t a, armdesk_rect_t b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static int test_linear_layout(void) {
    armdesk_linear_layout_t row =
        armdesk_row_layout(armdesk_rect(10, 20, 100, 30), 4);
    armdesk_linear_layout_t column =
        armdesk_column_layout(armdesk_rect(4, 5, 40, 50), 3);
    armdesk_rect_t item;
    armdesk_rect_t before;

    CHECK(armdesk_linear_layout_take(&row, 20, &item) == 0);
    CHECK(rect_equal(item, armdesk_rect(10, 20, 20, 30)));
    CHECK(rect_equal(row.remaining, armdesk_rect(34, 20, 76, 30)));

    CHECK(armdesk_linear_layout_take(&row, 76, &item) == 0);
    CHECK(rect_equal(item, armdesk_rect(34, 20, 76, 30)));
    CHECK(armdesk_rect_is_empty(row.remaining));

    before = row.remaining;
    CHECK(armdesk_linear_layout_take(&row, 1, &item) != 0);
    CHECK(rect_equal(row.remaining, before));

    CHECK(armdesk_linear_layout_take(&column, 10, &item) == 0);
    CHECK(rect_equal(item, armdesk_rect(4, 5, 40, 10)));
    CHECK(rect_equal(column.remaining, armdesk_rect(4, 18, 40, 37)));

    CHECK(armdesk_linear_layout_take(&column, 20, &item) == 0);
    CHECK(rect_equal(item, armdesk_rect(4, 18, 40, 20)));
    CHECK(rect_equal(column.remaining, armdesk_rect(4, 41, 40, 14)));

    before = column.remaining;
    CHECK(armdesk_linear_layout_take(&column, 15, &item) != 0);
    CHECK(rect_equal(column.remaining, before));
    return 0;
}

static int test_widget_models(void) {
    armdesk_label_t label = armdesk_label(
        armdesk_rect(8, 6, 80, 12), "Status", ARMDESK_THEME_STATUS_FG);
    armdesk_button_t button =
        armdesk_button(armdesk_rect(20, 30, 60, 18), "Save");

    CHECK(rect_equal(label.bounds, armdesk_rect(8, 6, 80, 12)));
    CHECK(label.text != 0 && label.text[0] == 'S');
    CHECK(label.color == ARMDESK_THEME_STATUS_FG);

    CHECK(button.enabled != 0U);
    CHECK(armdesk_button_hit_test(&button, 20, 30));
    CHECK(armdesk_button_hit_test(&button, 79, 47));
    CHECK(!armdesk_button_hit_test(&button, 80, 47));
    CHECK(!armdesk_button_hit_test(&button, 79, 48));

    button.hovered = 1U;
    button.pressed = 1U;
    armdesk_button_set_enabled(&button, 0);
    CHECK(button.enabled == 0U);
    CHECK(button.hovered == 0U);
    CHECK(button.pressed == 0U);
    CHECK(!armdesk_button_hit_test(&button, 20, 30));

    armdesk_button_set_enabled(&button, 1);
    CHECK(armdesk_button_hit_test(&button, 20, 30));
    return 0;
}

static int test_pointer_conversion(void) {
    armdesk_rect_t window = armdesk_rect(100, 50, 200, 120);
    armdesk_point_t point = {-1, -1};

    CHECK(armdesk_pointer_to_content(window, 16, 100, 66, &point) == 0);
    CHECK(point.x == 0 && point.y == 0);

    CHECK(armdesk_pointer_to_content(window, 16, 299, 169, &point) == 0);
    CHECK(point.x == 199 && point.y == 103);

    point.x = 7;
    point.y = 9;
    CHECK(armdesk_pointer_to_content(window, 16, 120, 65, &point) != 0);
    CHECK(point.x == 7 && point.y == 9);
    CHECK(armdesk_pointer_to_content(window, 16, 300, 80, &point) != 0);
    CHECK(armdesk_pointer_to_content(window, 16, 120, 170, &point) != 0);
    CHECK(armdesk_pointer_to_content(window, 120, 120, 80, &point) != 0);
    CHECK(armdesk_pointer_to_content(window, 16, 120, 80, 0) != 0);
    return 0;
}

int main(void) {
    int result = test_linear_layout();
    if (result != 0) return result;
    result = test_widget_models();
    if (result != 0) return result;
    return test_pointer_conversion();
}
