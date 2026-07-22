#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "fb/fb.h"
#include "input/input.h"
#include "kernel/gui_compositor.h"
#include "kernel/irq.h"
#include "kernel/process.h"
#include "kernel/runtime_service.h"
#include "net/virtio_net.h"

static uint64_t g_counter_values[16];
static uint32_t g_counter_count;
static uint32_t g_counter_index;
static uint32_t g_periodic_calls;
static uint32_t g_network_calls;
static uint32_t g_periodic_boundary_check;
static uint32_t g_render_calls;
static uint32_t g_fake_network_frames;
static uint32_t g_network_frames_returned;
static virtio_net_device_t g_net_device;
static uint8_t g_net_buffer[64];

uint32_t board_irq_ack(void) { return 0U; }
void board_irq_end(uint32_t irq) { (void)irq; }
int board_irq_is_spurious(uint32_t irq) { (void)irq; return 0; }
process_t *process_current(void) { return 0; }
gui_desktop_t *gui_desktop(void) { return 0; }
void gui_render(fb_t *fb, void *context) {
    (void)fb;
    (void)context;
    g_render_calls++;
}
void gui_clear_dirty(void) { }

void process_save_context(process_t *process, const uint64_t regs[31],
                          uint64_t pc, uint64_t pstate, uint64_t sp) {
    (void)process; (void)regs; (void)pc; (void)pstate; (void)sp;
}

int process_dispatch_next(process_t *current, exception_frame_t *frame,
                          process_dispatch_policy_t policy) {
    (void)current; (void)frame; (void)policy;
    return 0;
}

int input_queue_poll(input_event_t *event) { (void)event; return -1; }
int input_queue_available(void) { return 0; }

uint64_t runtime_service_counter_now(void) {
    assert(g_counter_index < g_counter_count);
    return g_counter_values[g_counter_index++];
}

int virtio_net_recv(virtio_net_device_t *device, void *data, uint32_t max_len) {
    (void)data;
    (void)max_len;
    assert(device == &g_net_device);
    if (g_fake_network_frames == 0U) {
        return 0;
    }
    g_fake_network_frames--;
    g_network_frames_returned++;
    runtime_service_report_metric(RUNTIME_METRIC_NETWORK_FRAMES, 1U);
    return 60;
}

void net_poll(void) {
    while (runtime_service_virtio_net_recv(&g_net_device, g_net_buffer,
                                           sizeof(g_net_buffer)) > 0) {
    }
}

void kernel_io_poll_network(void) {
    g_network_calls++;
    runtime_service_net_poll();
}

void kernel_on_timer_tick(void) {
    g_periodic_calls++;
    if (g_periodic_boundary_check != 0U) {
        runtime_service_gui_render(0, 0);
    }
}

static runtime_service_stats_t snapshot(void) {
    runtime_service_stats_t stats;

    runtime_service_get_stats(&stats);
    return stats;
}

static void prepare(const uint64_t *values, uint32_t count) {
    runtime_service_configure_timing(1000U, 10U);
    runtime_service_reset();
    assert(count <= (uint32_t)(sizeof(g_counter_values) /
                               sizeof(g_counter_values[0])));
    for (uint32_t i = 0; i < count; i++) {
        g_counter_values[i] = values[i];
    }
    g_counter_count = count;
    g_counter_index = 0U;
    g_periodic_calls = 0U;
    g_network_calls = 0U;
    g_periodic_boundary_check = 0U;
    g_render_calls = 0U;
    g_fake_network_frames = 0U;
    g_network_frames_returned = 0U;
}

static void periodic_expiry_defers_later_network(void) {
    static const uint64_t values[] = {100U, 111U, 112U};
    runtime_service_stats_t stats;

    prepare(values, 3U);
    g_periodic_boundary_check = 1U;
    runtime_service_request(RUNTIME_WORK_PERIODIC | RUNTIME_WORK_NETWORK);
    assert(runtime_service_run_pending() ==
           (RUNTIME_WORK_PERIODIC | RUNTIME_WORK_NETWORK));
    stats = snapshot();

    assert(g_periodic_calls == 1U);
    assert(g_render_calls == 0U);
    assert(g_network_calls == 0U);
    assert(stats.pending_work ==
           (RUNTIME_WORK_PERIODIC | RUNTIME_WORK_NETWORK));
    assert(stats.over_budget_count == 1U);
    assert(stats.requeue_count == 1U);
    assert(stats.last_duration_ticks == 12U);
    assert(g_counter_index == g_counter_count);
}

static void network_loop_stops_at_deadline(void) {
    static const uint64_t values[] = {100U, 101U, 105U,
                                      109U, 110U, 111U};
    runtime_service_stats_t stats;

    prepare(values, 6U);
    g_fake_network_frames = 5U;
    runtime_service_request(RUNTIME_WORK_NETWORK);
    assert(runtime_service_run_pending() == RUNTIME_WORK_NETWORK);
    stats = snapshot();

    assert(g_network_calls == 1U);
    assert(g_network_frames_returned == 3U);
    assert(g_fake_network_frames == 2U);
    assert(stats.metric_last[RUNTIME_METRIC_NETWORK_FRAMES] == 3U);
    assert(stats.pending_work == RUNTIME_WORK_NETWORK);
    assert(stats.over_budget_count == 1U);
    assert(stats.requeue_count == 1U);
    assert(stats.last_duration_ticks == 11U);
    assert(g_counter_index == g_counter_count);
}

static void completed_operation_overrun_is_rechecked(void) {
    static const uint64_t values[] = {100U, 111U, 115U};
    runtime_service_stats_t stats;

    prepare(values, 3U);
    runtime_service_request(RUNTIME_WORK_PERIODIC);
    assert(runtime_service_run_pending() == RUNTIME_WORK_PERIODIC);
    stats = snapshot();

    assert(g_periodic_calls == 1U);
    assert(stats.pending_work == RUNTIME_WORK_PERIODIC);
    assert(stats.over_budget_count == 1U);
    assert(stats.requeue_count == 1U);
    assert(stats.last_duration_ticks == 15U);
    assert(g_counter_index == g_counter_count);
}

int main(void) {
    periodic_expiry_defers_later_network();
    network_loop_stops_at_deadline();
    completed_operation_overrun_is_rechecked();
    puts("deferred runtime service global deadline: ok");
    return 0;
}
