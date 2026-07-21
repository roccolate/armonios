#ifndef ARMONIOS_DRIVERS_INPUT_INPUT_H
#define ARMONIOS_DRIVERS_INPUT_INPUT_H

#include <stdint.h>

#define INPUT_EVENT_QUEUE_SIZE 64

#define INPUT_KEY_UP     0x101U
#define INPUT_KEY_DOWN   0x102U
#define INPUT_KEY_LEFT   0x103U
#define INPUT_KEY_RIGHT  0x104U
#define INPUT_KEY_PGUP   0x105U
#define INPUT_KEY_PGDN   0x106U
#define INPUT_KEY_HOME   0x107U
#define INPUT_KEY_END    0x108U
#define INPUT_KEY_INSERT 0x109U
#define INPUT_KEY_DELETE 0x10aU

typedef enum {
    INPUT_EVENT_KEY_PRESS,
    INPUT_EVENT_KEY_RELEASE,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_BUTTON,
} input_event_type_t;

typedef struct input_event {
    input_event_type_t type;
    uint64_t timestamp;
    union {
        struct { uint32_t key; } key;
        struct { int32_t dx; int32_t dy; } mouse_move;
        struct { uint32_t button; uint32_t pressed; } mouse_button;
    } data;
} input_event_t;

typedef struct {
    uint64_t overflow_count;
    uint32_t current_depth;
    uint32_t high_water;
} input_queue_stats_t;

void input_queue_init(void);
int input_queue_push(const input_event_t *event);
int input_queue_poll(input_event_t *event);
int input_queue_peek(input_event_t *event);
int input_queue_poll_char(void);
int input_queue_available(void);
void input_queue_get_stats(input_queue_stats_t *stats);
int input_uart_poll(void);
int input_inject_byte(int c);

#endif
