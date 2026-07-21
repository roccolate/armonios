#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "fb/fb.h"
#include "kernel/gui_compositor.h"
#include "kernel/runtime_service.h"

static gui_desktop_t g_desktop;
static uint32_t g_dirty;
static uint32_t g_draw_success;
static uint32_t g_draw_calls;
static uint32_t g_clear_calls;
static uint32_t g_draw_damage_seen;
static uint32_t g_draw_full_seen;
static uint64_t g_counter_now;

gui_desktop_t *gui_desktop(void) {
    return &g_desktop;
}

void gui_render(fb_t *fb, void *context) {
    (void)fb;
    (void)context;
    g_draw_calls++;
    g_draw_damage_seen = g_desktop.damage_count;
    g_draw_full_seen = g_desktop.damage_full;
}

void gui_clear_dirty(void) {
    g_clear_calls++;
    g_dirty = 0U;
    g_desktop.damage_count = 0U;
    g_desktop.damage_full = 0U;
}

uint64_t runtime_service_counter_now(void) {
    return g_counter_now++;
}

void kernel_on_timer_tick(void) {
    runtime_service_gui_render(0, 0);
    if (g_draw_success != 0U) {
        runtime_service_report_redraw();
    }
    runtime_service_gui_clear_dirty();
}

static runtime_service_stats_t snapshot(void) {
    runtime_service_stats_t stats;

    runtime_service_get_stats(&stats);
    return stats;
}

static void prepare(uint32_t damage_count, uint32_t full, uint32_t success) {
    runtime_service_configure_timing(1000U, 10U);
    runtime_service_reset();
    g_desktop.damage_count = damage_count;
    g_desktop.damage_full = (uint8_t)full;
    for (uint32_t i = 0; i < damage_count; i++) {
        g_desktop.damage_rects[i].x = (int32_t)i;
        g_desktop.damage_rects[i].y = 0;
        g_desktop.damage_rects[i].w = 1;
        g_desktop.damage_rects[i].h = 1;
    }
    g_dirty = 1U;
    g_draw_success = success;
    g_draw_calls = 0U;
    g_clear_calls = 0U;
    g_draw_damage_seen = 0U;
    g_draw_full_seen = 0U;
    g_counter_now = 100U;
}

static void twenty_rectangles_complete_as_eight_eight_four(void) {
    runtime_service_stats_t stats;

    prepare(20U, 0U, 1U);

    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    stats = snapshot();
    assert(g_draw_damage_seen == RUNTIME_REDRAW_DAMAGE_BUDGET);
    assert(g_desktop.damage_count == 12U);
    assert(g_desktop.damage_rects[0].x == 8);
    assert(g_dirty == 1U);
    assert(g_clear_calls == 0U);
    assert(stats.metric_last[RUNTIME_METRIC_REDRAW] == 1U);
    assert(stats.metric_last[RUNTIME_METRIC_DAMAGE_ITEMS] == 8U);
    assert(stats.redraw_budget_exhaustion_count == 1U);

    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    stats = snapshot();
    assert(g_draw_damage_seen == RUNTIME_REDRAW_DAMAGE_BUDGET);
    assert(g_desktop.damage_count == 4U);
    assert(g_desktop.damage_rects[0].x == 16);
    assert(g_dirty == 1U);
    assert(g_clear_calls == 0U);
    assert(stats.metric_last[RUNTIME_METRIC_DAMAGE_ITEMS] == 8U);
    assert(stats.metric_total[RUNTIME_METRIC_DAMAGE_ITEMS] == 16U);
    assert(stats.redraw_budget_exhaustion_count == 2U);

    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    stats = snapshot();
    assert(g_draw_damage_seen == 4U);
    assert(g_desktop.damage_count == 0U);
    assert(g_dirty == 0U);
    assert(g_clear_calls == 1U);
    assert(stats.metric_last[RUNTIME_METRIC_DAMAGE_ITEMS] == 4U);
    assert(stats.metric_total[RUNTIME_METRIC_DAMAGE_ITEMS] == 20U);
    assert(stats.metric_total[RUNTIME_METRIC_REDRAW] == 3U);
    assert(stats.redraw_budget_exhaustion_count == 2U);
}

static void failed_redraw_preserves_all_damage(void) {
    runtime_service_stats_t stats;

    prepare(5U, 0U, 0U);
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    stats = snapshot();

    assert(g_draw_calls == 1U);
    assert(g_draw_damage_seen == 5U);
    assert(g_desktop.damage_count == 5U);
    assert(g_desktop.damage_rects[0].x == 0);
    assert(g_dirty == 1U);
    assert(g_clear_calls == 0U);
    assert(stats.metric_total[RUNTIME_METRIC_REDRAW] == 0U);
    assert(stats.metric_total[RUNTIME_METRIC_DAMAGE_ITEMS] == 0U);
    assert(stats.redraw_budget_exhaustion_count == 0U);

    g_draw_success = 1U;
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    stats = snapshot();
    assert(g_desktop.damage_count == 0U);
    assert(g_dirty == 0U);
    assert(g_clear_calls == 1U);
    assert(stats.metric_total[RUNTIME_METRIC_REDRAW] == 1U);
    assert(stats.metric_total[RUNTIME_METRIC_DAMAGE_ITEMS] == 5U);
}

static void full_redraw_is_one_successful_operation(void) {
    runtime_service_stats_t stats;

    prepare(0U, 1U, 1U);
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    stats = snapshot();

    assert(g_draw_calls == 1U);
    assert(g_draw_full_seen == 1U);
    assert(g_desktop.damage_full == 0U);
    assert(g_dirty == 0U);
    assert(g_clear_calls == 1U);
    assert(stats.metric_last[RUNTIME_METRIC_REDRAW] == 1U);
    assert(stats.metric_last[RUNTIME_METRIC_FULL_REDRAWS] == 1U);
    assert(stats.metric_last[RUNTIME_METRIC_DAMAGE_ITEMS] == 0U);
    assert(stats.redraw_budget_exhaustion_count == 0U);
}

static void reset_clears_redraw_exhaustion(void) {
    prepare(20U, 0U, 1U);
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    (void)runtime_service_run_pending();
    assert(snapshot().redraw_budget_exhaustion_count == 1U);

    runtime_service_reset();
    assert(snapshot().redraw_budget_exhaustion_count == 0U);
}

int main(void) {
    twenty_rectangles_complete_as_eight_eight_four();
    failed_redraw_preserves_all_damage();
    full_redraw_is_one_successful_operation();
    reset_clears_redraw_exhaustion();
    puts("deferred runtime service redraw budget: ok");
    return 0;
}
