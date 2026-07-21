#ifndef ARMONIOS_KERNEL_RUNTIME_SERVICE_H
#define ARMONIOS_KERNEL_RUNTIME_SERVICE_H

#include <stdint.h>

/* Initial count budgets derived from fixed queue/ring capacities. */
#define RUNTIME_INPUT_EVENT_BUDGET 16U
#define RUNTIME_NETWORK_FRAME_BUDGET 16U
#define RUNTIME_REDRAW_DAMAGE_BUDGET 8U

/* Deferred work published from hard IRQ and consumed after EOI. */
enum {
    RUNTIME_WORK_PERIODIC = 1U << 0,
    RUNTIME_WORK_INPUT = 1U << 1,
    RUNTIME_WORK_NETWORK = 1U << 2,
    RUNTIME_WORK_ALL = RUNTIME_WORK_PERIODIC | RUNTIME_WORK_INPUT |
                       RUNTIME_WORK_NETWORK,
};

/* Work classes measured during one active runtime-service pass. */
enum {
    RUNTIME_METRIC_INPUT_PRODUCED = 0,
    RUNTIME_METRIC_INPUT_CONSUMED,
    RUNTIME_METRIC_REDRAW,
    RUNTIME_METRIC_NETWORK_FRAMES,
    RUNTIME_METRIC_DEVICE_POLLS,
    RUNTIME_METRIC_DAMAGE_ITEMS,
    RUNTIME_METRIC_FULL_REDRAWS,
    RUNTIME_METRIC_COUNT,
};

typedef struct {
    uint64_t request_count;
    uint64_t coalesced_request_count;
    uint64_t run_count;
    uint64_t empty_run_count;
    uint64_t requeue_count;
    uint64_t last_duration_ticks;
    uint64_t max_duration_ticks;
    uint64_t total_duration_ticks;
    uint64_t over_budget_count;
    uint64_t input_budget_exhaustion_count;
    uint64_t network_budget_exhaustion_count;
    uint64_t redraw_budget_exhaustion_count;
    uint64_t counter_frequency_hz;
    uint64_t budget_ticks;

    uint64_t metric_total[RUNTIME_METRIC_COUNT];
    uint64_t input_queue_overflow_count;
    uint32_t metric_last[RUNTIME_METRIC_COUNT];
    uint32_t metric_max[RUNTIME_METRIC_COUNT];
    uint32_t max_input_queue_depth;
    uint32_t input_queue_high_water;

    uint32_t pending_work;
    uint32_t last_work;
} runtime_service_stats_t;

struct fb;
struct input_event;

void runtime_service_request(uint32_t work);
uint32_t runtime_service_run_pending(void);
void runtime_service_reset(void);

void runtime_service_configure_timing(uint64_t counter_frequency_hz,
                                      uint64_t budget_ticks);
void runtime_service_report_metric(uint32_t metric, uint32_t value);
void runtime_service_report_redraw(void);
void runtime_service_report_input_queue(uint32_t depth, uint32_t high_water,
                                        uint64_t overflow_count);
void runtime_service_get_stats(runtime_service_stats_t *stats);

/* Wrappers used by kernel orchestration and deterministic host tests. */
int runtime_service_input_poll(struct input_event *event);
void runtime_service_net_poll(void);
void runtime_service_gui_render(struct fb *fb, void *context);
void runtime_service_gui_clear_dirty(void);

/* Weak zero clock in irq.c; strong CNTPCT_EL0 implementation in timer.c. */
uint64_t runtime_service_counter_now(void);

#endif
