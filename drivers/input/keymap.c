#include "input/keymap.h"

#include "input/input.h"

uint32_t input_key_from_nav(input_nav_key_t nav) {
    switch (nav) {
    case INPUT_NAV_UP: return INPUT_KEY_UP;
    case INPUT_NAV_DOWN: return INPUT_KEY_DOWN;
    case INPUT_NAV_LEFT: return INPUT_KEY_LEFT;
    case INPUT_NAV_RIGHT: return INPUT_KEY_RIGHT;
    case INPUT_NAV_PGUP: return INPUT_KEY_PGUP;
    case INPUT_NAV_PGDN: return INPUT_KEY_PGDN;
    case INPUT_NAV_HOME: return INPUT_KEY_HOME;
    case INPUT_NAV_END: return INPUT_KEY_END;
    case INPUT_NAV_INSERT: return INPUT_KEY_INSERT;
    case INPUT_NAV_DELETE: return INPUT_KEY_DELETE;
    default: return 0;
    }
}

uint32_t input_key_from_ctrl_letter(uint8_t letter) {
    if (letter >= 'a' && letter <= 'z') {
        return (uint32_t)(letter - 'a' + 1U);
    }
    if (letter >= 'A' && letter <= 'Z') {
        return (uint32_t)(letter - 'A' + 1U);
    }
    return 0;
}
