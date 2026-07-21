#include "input.h"

#include "keymap.h"
#include "kernel/irq.h"
#include "kernel/runtime_service.h"
#include "uart/pl011.h"

static input_event_t g_event_queue[INPUT_EVENT_QUEUE_SIZE];
static uint32_t g_event_head;
static uint32_t g_event_tail;
static uint32_t g_event_count;
static uint32_t g_event_high_water;
static uint64_t g_event_overflow_count;

enum {
    ESC_STATE_IDLE = 0,
    ESC_STATE_GOT_ESC,
    ESC_STATE_GOT_BRACKET,
    ESC_STATE_GOT_BRACKET_DIGIT,
};

static uint8_t g_esc_state;
static uint8_t g_esc_digit;

static void push_key_event(uint32_t key) {
    input_event_t event = {
        .type = INPUT_EVENT_KEY_PRESS,
        .timestamp = 0,
        .data.key.key = key,
    };
    (void)input_queue_push(&event);
}

static void push_escape_key(uint8_t code) {
    input_nav_key_t nav;

    switch (code) {
    case 'A': nav = INPUT_NAV_UP; break;
    case 'B': nav = INPUT_NAV_DOWN; break;
    case 'C': nav = INPUT_NAV_RIGHT; break;
    case 'D': nav = INPUT_NAV_LEFT; break;
    case 'F': nav = INPUT_NAV_END; break;
    case 'H': nav = INPUT_NAV_HOME; break;
    default: return;
    }
    push_key_event(input_key_from_nav(nav));
}

static void push_escape_tilde(uint8_t digit) {
    input_nav_key_t nav;

    switch (digit) {
    case '1': case '7': nav = INPUT_NAV_HOME; break;
    case '2': nav = INPUT_NAV_INSERT; break;
    case '3': nav = INPUT_NAV_DELETE; break;
    case '4': case '8': nav = INPUT_NAV_END; break;
    case '5': nav = INPUT_NAV_PGUP; break;
    case '6': nav = INPUT_NAV_PGDN; break;
    default: return;
    }
    push_key_event(input_key_from_nav(nav));
}

void input_queue_init(void) {
    g_event_head = 0;
    g_event_tail = 0;
    g_event_count = 0;
    g_event_high_water = 0;
    g_event_overflow_count = 0;
    g_esc_state = ESC_STATE_IDLE;
    g_esc_digit = 0;
}

int input_queue_push(const input_event_t *event) {
    if (event == 0) {
        return -1;
    }

    irq_disable();
    if (g_event_count >= INPUT_EVENT_QUEUE_SIZE) {
        g_event_overflow_count++;
        irq_enable();
        return -1;
    }

    g_event_queue[g_event_tail] = *event;
    g_event_tail = (g_event_tail + 1U) % INPUT_EVENT_QUEUE_SIZE;
    g_event_count++;
    if (g_event_count > g_event_high_water) {
        g_event_high_water = g_event_count;
    }
    irq_enable();
    return 0;
}

int input_queue_poll(input_event_t *event) {
    uint32_t depth;
    uint32_t high_water;
    uint64_t overflow;

    if (event == 0) {
        return -1;
    }

    irq_disable();
    if (g_event_count == 0U) {
        irq_enable();
        return -1;
    }

    depth = g_event_count;
    high_water = g_event_high_water;
    overflow = g_event_overflow_count;
    *event = g_event_queue[g_event_head];
    g_event_head = (g_event_head + 1U) % INPUT_EVENT_QUEUE_SIZE;
    g_event_count--;
    irq_enable();

    runtime_service_report_metric(RUNTIME_METRIC_INPUT_CONSUMED, 1U);
    runtime_service_report_input_queue(depth, high_water, overflow);
    return 0;
}

int input_queue_peek(input_event_t *event) {
    if (event == 0) {
        return -1;
    }
    irq_disable();
    if (g_event_count == 0U) {
        irq_enable();
        return -1;
    }
    *event = g_event_queue[g_event_head];
    irq_enable();
    return 0;
}

int input_queue_poll_char(void) {
    input_event_t event;

    if (input_queue_poll(&event) != 0 || event.type != INPUT_EVENT_KEY_PRESS) {
        return -1;
    }
    return (int)(event.data.key.key & 0xffU);
}

int input_queue_available(void) {
    int count;

    irq_disable();
    count = (int)g_event_count;
    irq_enable();
    return count;
}

void input_queue_get_stats(input_queue_stats_t *stats) {
    if (stats == 0) {
        return;
    }
    irq_disable();
    stats->overflow_count = g_event_overflow_count;
    stats->current_depth = g_event_count;
    stats->high_water = g_event_high_water;
    irq_enable();
}

int input_uart_poll(void) {
    (void)uart_pump_input;
    return -1;
}

int input_inject_byte(int c) {
    if (g_esc_state == ESC_STATE_IDLE) {
        if (c == 0x1B) {
            g_esc_state = ESC_STATE_GOT_ESC;
        } else {
            push_key_event((uint32_t)c);
        }
        return 0;
    }

    if (g_esc_state == ESC_STATE_GOT_ESC) {
        if (c == '[') {
            g_esc_state = ESC_STATE_GOT_BRACKET;
            return 0;
        }
        g_esc_state = ESC_STATE_IDLE;
        push_key_event(0x1BU);
        if (c == 0x1B) {
            g_esc_state = ESC_STATE_GOT_ESC;
        } else {
            push_key_event((uint32_t)c);
        }
        return 0;
    }

    if (g_esc_state == ESC_STATE_GOT_BRACKET) {
        if (c >= '0' && c <= '9') {
            g_esc_state = ESC_STATE_GOT_BRACKET_DIGIT;
            g_esc_digit = (uint8_t)c;
        } else {
            g_esc_state = ESC_STATE_IDLE;
            push_escape_key((uint8_t)c);
        }
        return 0;
    }

    g_esc_state = ESC_STATE_IDLE;
    if (c == '~') {
        push_escape_tilde(g_esc_digit);
    }
    return 0;
}
