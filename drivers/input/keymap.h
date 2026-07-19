#ifndef ARMONIOS_DRIVERS_INPUT_KEYMAP_H
#define ARMONIOS_DRIVERS_INPUT_KEYMAP_H

#include <stdint.h>

typedef enum {
    INPUT_NAV_UP = 0,
    INPUT_NAV_DOWN,
    INPUT_NAV_LEFT,
    INPUT_NAV_RIGHT,
    INPUT_NAV_PGUP,
    INPUT_NAV_PGDN,
    INPUT_NAV_HOME,
    INPUT_NAV_END,
    INPUT_NAV_INSERT,
    INPUT_NAV_DELETE,
} input_nav_key_t;

uint32_t input_key_from_nav(input_nav_key_t nav);
uint32_t input_key_from_ctrl_letter(uint8_t letter);

#endif
