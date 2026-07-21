#include "input/virtio_input.h"

#include <stddef.h>
#include <stdint.h>

#include "input/input.h"
#include "input/keymap.h"
#include "kernel/kernel_compiler.h"
#include "uart/pl011.h"

#define VIRTIO_MMIO_MAGIC_VALUE         0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_VENDOR_ID           0x00c
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
#define VIRTIO_MMIO_QUEUE_READY         0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050
#define VIRTIO_MMIO_STATUS              0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW      0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084
#define VIRTIO_MMIO_QUEUE_DRIVER_LOW    0x090
#define VIRTIO_MMIO_QUEUE_DRIVER_HIGH   0x094
#define VIRTIO_MMIO_QUEUE_DEVICE_LOW    0x0a0
#define VIRTIO_MMIO_QUEUE_DEVICE_HIGH   0x0a4
#define VIRTIO_MMIO_CONFIG              0x100
#define VIRTIO_MMIO_INT_STATUS           0x60
#define VIRTIO_MMIO_INT_ENABLE           0x064

#define VIRTIO_MMIO_MAGIC               0x74726976U
#define VIRTIO_DEVICE_ID_INPUT          18U
#define VIRTIO_STATUS_ACKNOWLEDGE       1U
#define VIRTIO_STATUS_DRIVER            2U
#define VIRTIO_STATUS_DRIVER_OK         4U
#define VIRTIO_STATUS_FAILED            128U
#define VIRTQ_DESC_F_NEXT               1U
#define VIRTQ_DESC_F_WRITE              2U
#define VIRTIO_INPUT_QUEUE_SIZE         VIRTIO_INPUT_POLL_BUDGET

#define VIRTIO_INPUT_EVENTQ             0

typedef struct {
    uint16_t type;
    uint16_t code;
    int32_t value;
} virtio_input_event_t;

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} KERNEL_PACKED virtq_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_INPUT_QUEUE_SIZE];
} KERNEL_PACKED virtq_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} KERNEL_PACKED virtq_used_elem_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[VIRTIO_INPUT_QUEUE_SIZE];
} KERNEL_PACKED virtq_used_t;

static virtq_desc_t g_desc[VIRTIO_INPUT_QUEUE_SIZE] KERNEL_ALIGNED(16);
static virtq_avail_t g_avail KERNEL_ALIGNED(2);
static virtq_used_t g_used KERNEL_ALIGNED(4);
static virtio_input_event_t g_events[VIRTIO_INPUT_QUEUE_SIZE] KERNEL_ALIGNED(8);
static uint8_t g_shift_down;
static uint8_t g_ctrl_down;
static uint32_t g_pressed_values[128];

static uint8_t linux_letter(uint16_t code) {
    static const char top[] = "qwertyuiop";
    static const char home[] = "asdfghjkl";
    static const char bottom[] = "zxcvbnm";

    if (code >= 16U && code <= 25U) {
        return (uint8_t)top[code - 16U];
    }
    if (code >= 30U && code <= 38U) {
        return (uint8_t)home[code - 30U];
    }
    if (code >= 44U && code <= 50U) {
        return (uint8_t)bottom[code - 44U];
    }
    return 0;
}

static uint8_t shift_digit(uint8_t digit) {
    static const char shifted[] = ")!@#$%^&*(";

    if (digit >= '0' && digit <= '9') {
        return (uint8_t)shifted[digit - '0'];
    }
    return digit;
}

static uint8_t linux_key_ascii(uint16_t code, uint8_t shifted) {
    uint8_t letter = linux_letter(code);
    if (letter != 0) {
        return shifted ? (uint8_t)(letter - ('a' - 'A')) : letter;
    }
    if (code >= 2U && code <= 10U) {
        uint8_t digit = (uint8_t)('1' + code - 2U);
        return shifted ? shift_digit(digit) : digit;
    }

    switch (code) {
    case 1U: return 0x1BU;
    case 11U: return shifted ? ')' : '0';
    case 12U: return shifted ? '_' : '-';
    case 13U: return shifted ? '+' : '=';
    case 14U: return '\b';
    case 15U: return '\t';
    case 26U: return shifted ? '{' : '[';
    case 27U: return shifted ? '}' : ']';
    case 28U: return '\n';
    case 39U: return shifted ? ':' : ';';
    case 40U: return shifted ? '"' : '\'';
    case 41U: return shifted ? '~' : '`';
    case 43U: return shifted ? '|' : '\\';
    case 51U: return shifted ? '<' : ',';
    case 52U: return shifted ? '>' : '.';
    case 53U: return shifted ? '?' : '/';
    case 57U: return ' ';
    case 71U: return '7';
    case 72U: return '8';
    case 73U: return '9';
    case 74U: return '-';
    case 75U: return '4';
    case 76U: return '5';
    case 77U: return '6';
    case 78U: return '+';
    case 79U: return '1';
    case 80U: return '2';
    case 81U: return '3';
    case 82U: return '0';
    case 83U: return '.';
    case 96U: return '\n';
    default: return 0;
    }
}

uint32_t virtio_input_key_to_input_key(uint16_t code, uint8_t shifted,
                                       uint8_t ctrl) {
    if (ctrl) {
        uint8_t base = linux_letter(code);
        if (base != 0) {
            return input_key_from_ctrl_letter(base);
        }
    }

    switch (code) {
    case 102U: return input_key_from_nav(INPUT_NAV_HOME);
    case 103U: return input_key_from_nav(INPUT_NAV_UP);
    case 104U: return input_key_from_nav(INPUT_NAV_PGUP);
    case 105U: return input_key_from_nav(INPUT_NAV_LEFT);
    case 106U: return input_key_from_nav(INPUT_NAV_RIGHT);
    case 107U: return input_key_from_nav(INPUT_NAV_END);
    case 108U: return input_key_from_nav(INPUT_NAV_DOWN);
    case 109U: return input_key_from_nav(INPUT_NAV_PGDN);
    case 110U: return input_key_from_nav(INPUT_NAV_INSERT);
    case 111U: return input_key_from_nav(INPUT_NAV_DELETE);
    default:
        break;
    }

    return linux_key_ascii(code, shifted);
}

static uint8_t virtio_input_modifier_code(uint16_t code, uint8_t *modifier) {
    if (modifier == 0) {
        return 0;
    }
    if (code == VIRTIO_INPUT_KEY_LEFTSHIFT ||
        code == VIRTIO_INPUT_KEY_RIGHTSHIFT) {
        *modifier = 1;
        return 1;
    }
    if (code == VIRTIO_INPUT_KEY_LEFTCTRL ||
        code == VIRTIO_INPUT_KEY_RIGHTCTRL) {
        *modifier = 2;
        return 1;
    }
    return 0;
}

static volatile uint32_t *virtio_reg(uint64_t base, uint32_t offset) {
    return (volatile uint32_t *)(uintptr_t)(base + offset);
}

static uint32_t virtio_read32(uint64_t base, uint32_t offset) {
    return *virtio_reg(base, offset);
}

static void virtio_write32(uint64_t base, uint32_t offset, uint32_t value) {
    *virtio_reg(base, offset) = value;
}

static void virtio_write64(uint64_t base, uint32_t offset_low, uint32_t offset_high, uint64_t value) {
    virtio_write32(base, offset_low, (uint32_t)value);
    virtio_write32(base, offset_high, (uint32_t)(value >> 32));
}

static void mb(void) {
#if defined(__aarch64__)
    __asm__ volatile("dmb sy" ::: "memory");
#else
    __sync_synchronize();
#endif
}

int virtio_input_probe(uint64_t base) {
    uint32_t magic = virtio_read32(base, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MMIO_MAGIC) {
        return -1;
    }

    uint32_t version = virtio_read32(base, VIRTIO_MMIO_VERSION);
    if (version != 2) {
        return -1;
    }

    uint32_t device_id = virtio_read32(base, VIRTIO_MMIO_DEVICE_ID);
    if (device_id != VIRTIO_DEVICE_ID_INPUT) {
        return -1;
    }

    return 0;
}

int virtio_input_probe_range(uint64_t base, uint64_t size, uint64_t stride,
                             uint64_t *found_base) {
    if (base == 0 || size == 0 || stride == 0 || found_base == 0) {
        return -1;
    }

    for (uint64_t offset = 0; offset < size; offset += stride) {
        uint64_t candidate = base + offset;
        if (virtio_input_probe(candidate) == 0) {
            *found_base = candidate;
            return 0;
        }
    }

    return -1;
}

static int setup_event_queue(uint64_t base, uint32_t *out_queue_size) {
    virtio_write32(base, VIRTIO_MMIO_QUEUE_SEL, VIRTIO_INPUT_EVENTQ);

    uint32_t qsize = virtio_read32(base, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (qsize == 0 || out_queue_size == 0) {
        return -1;
    }
    if (qsize > VIRTIO_INPUT_QUEUE_SIZE) {
        qsize = VIRTIO_INPUT_QUEUE_SIZE;
    }

    virtio_write32(base, VIRTIO_MMIO_QUEUE_NUM, qsize);

    for (size_t i = 0; i < VIRTIO_INPUT_QUEUE_SIZE; i++) {
        g_desc[i].addr = 0;
        g_desc[i].len = 0;
        g_desc[i].flags = 0;
        g_desc[i].next = 0;
    }
    g_avail.flags = 0;
    g_avail.idx = 0;
    g_used.flags = 0;
    g_used.idx = 0;

    for (uint32_t i = 0; i < qsize; i++) {
        g_desc[i].addr = (uint64_t)(uintptr_t)&g_events[i];
        g_desc[i].len = sizeof(virtio_input_event_t);
        g_desc[i].flags = VIRTQ_DESC_F_WRITE;
        g_desc[i].next = 0;
        g_avail.ring[i] = (uint16_t)i;
    }

    virtio_write64(base, VIRTIO_MMIO_QUEUE_DESC_LOW, VIRTIO_MMIO_QUEUE_DESC_HIGH,
                   (uint64_t)(uintptr_t)g_desc);
    virtio_write64(base, VIRTIO_MMIO_QUEUE_DRIVER_LOW, VIRTIO_MMIO_QUEUE_DRIVER_HIGH,
                   (uint64_t)(uintptr_t)&g_avail);
    virtio_write64(base, VIRTIO_MMIO_QUEUE_DEVICE_LOW, VIRTIO_MMIO_QUEUE_DEVICE_HIGH,
                   (uint64_t)(uintptr_t)&g_used);

    mb();
    g_avail.idx = (uint16_t)qsize;
    mb();

    virtio_write32(base, VIRTIO_MMIO_QUEUE_READY, 1);
    virtio_write32(base, VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_INPUT_EVENTQ);

    *out_queue_size = qsize;
    return 0;
}

int virtio_input_init(virtio_input_device_t *device, uint64_t base) {
    if (device == 0 || base == 0 || virtio_input_probe(base) != 0) {
        return -1;
    }

    device->base = base;
    device->ready = 0;
    device->queue_size = 0;
    device->last_used_idx = 0;
    g_shift_down = 0;
    g_ctrl_down = 0;
    for (uint32_t i = 0; i < 128U; i++) {
        g_pressed_values[i] = 0;
    }

    uint8_t status = 0;
    status |= VIRTIO_STATUS_ACKNOWLEDGE;
    virtio_write32(base, VIRTIO_MMIO_STATUS, status);

    status |= VIRTIO_STATUS_DRIVER;
    virtio_write32(base, VIRTIO_MMIO_STATUS, status);

    uint32_t qsize = 0;
    if (setup_event_queue(base, &qsize) != 0) {
        return -1;
    }

    status |= VIRTIO_STATUS_DRIVER_OK;
    virtio_write32(base, VIRTIO_MMIO_STATUS, status);
    virtio_write32(base, VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_INPUT_EVENTQ);

    device->ready = 1;
    device->queue_size = qsize;

    return 0;
}

int virtio_input_has_events(virtio_input_device_t *device) {
    if (device == 0 || !device->ready) {
        return 0;
    }

    mb();
    return g_used.idx != device->last_used_idx;
}

int virtio_input_poll(virtio_input_device_t *device) {
    uint32_t produced = 0;

    if (device == 0 || !device->ready || device->queue_size == 0U ||
        device->queue_size > VIRTIO_INPUT_QUEUE_SIZE) {
        return -1;
    }

    for (uint32_t processed = 0;
         processed < device->queue_size &&
         processed < VIRTIO_INPUT_POLL_BUDGET;
         processed++) {
        mb();
        if (device->last_used_idx == g_used.idx) {
            break;
        }

        uint16_t used_slot = device->last_used_idx % device->queue_size;
        uint32_t desc_id = g_used.ring[used_slot].id;

        if (desc_id >= device->queue_size) {
            device->last_used_idx++;
            continue;
        }

        virtio_input_event_t *ev = &g_events[desc_id];

        input_event_t event = {0};
        event.timestamp = 0;

        switch (ev->type) {
        case VIRTIO_INPUT_EV_SYN:
            break;
        case VIRTIO_INPUT_EV_KEY:
            /* Mouse buttons live in the same EV_KEY code range as keyboard
             * scancodes (0x110..0x112). Translate them to MOUSE_BUTTON so
             * the GUI layer does not have to know about Linux codes. */
            if (ev->code >= VIRTIO_INPUT_BTN_LEFT &&
                ev->code <= VIRTIO_INPUT_BTN_MIDDLE) {
                event.type = INPUT_EVENT_MOUSE_BUTTON;
                event.data.mouse_button.button =
                    (uint32_t)(ev->code - VIRTIO_INPUT_BTN_LEFT);
                event.data.mouse_button.pressed = (ev->value != 0) ? 1U : 0U;
                if (input_queue_push(&event) == 0) {
                    produced++;
                }
                break;
            }
            {
                uint8_t modifier = 0;
                if (virtio_input_modifier_code(ev->code, &modifier)) {
                    if (modifier == 1) {
                        g_shift_down = ev->value != 0 ? 1U : 0U;
                    } else {
                        g_ctrl_down = ev->value != 0 ? 1U : 0U;
                    }
                    break;
                }

                uint32_t key;
                if (ev->value != 0) {
                    key = virtio_input_key_to_input_key(
                        ev->code, g_shift_down, g_ctrl_down);
                    if (ev->code < 128U) {
                        g_pressed_values[ev->code] = key;
                    }
                    event.type = INPUT_EVENT_KEY_PRESS;
                } else {
                    key = ev->code < 128U ? g_pressed_values[ev->code] : 0;
                    if (key == 0) {
                        key = virtio_input_key_to_input_key(
                            ev->code, g_shift_down, g_ctrl_down);
                    }
                    if (ev->code < 128U) {
                        g_pressed_values[ev->code] = 0;
                    }
                    event.type = INPUT_EVENT_KEY_RELEASE;
                }
                if (key != 0) {
                    event.data.key.key = key;
                    if (input_queue_push(&event) == 0) {
                        produced++;
                    }
                }
            }
            break;
        case VIRTIO_INPUT_EV_REL:
            if (ev->code == 0 || ev->code == 1) {
                event.type = INPUT_EVENT_MOUSE_MOVE;
                if (ev->code == 0) {
                    event.data.mouse_move.dx = ev->value;
                    event.data.mouse_move.dy = 0;
                } else {
                    event.data.mouse_move.dx = 0;
                    event.data.mouse_move.dy = ev->value;
                }
                if (input_queue_push(&event) == 0) {
                    produced++;
                }
            }
            break;
        case VIRTIO_INPUT_EV_ABS:
            break;
        default:
            break;
        }

        device->last_used_idx++;

        uint16_t avail_slot = g_avail.idx % device->queue_size;
        g_avail.ring[avail_slot] = (uint16_t)desc_id;

        mb();
        g_avail.idx++;
        mb();
        virtio_write32(device->base, VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_INPUT_EVENTQ);
    }

    return (int)produced;
}

#ifdef ARMONIOS_TEST
void virtio_input_test_set_used_idx(uint16_t used_idx) {
    g_used.idx = used_idx;
}

void virtio_input_test_set_event(uint16_t used_slot, uint32_t desc_id,
                                 uint16_t type, uint16_t code, int32_t value) {
    if (used_slot >= VIRTIO_INPUT_QUEUE_SIZE ||
        desc_id >= VIRTIO_INPUT_QUEUE_SIZE) {
        return;
    }
    g_used.ring[used_slot].id = desc_id;
    g_events[desc_id].type = type;
    g_events[desc_id].code = code;
    g_events[desc_id].value = value;
}
#endif
