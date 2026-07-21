#ifndef ARMONIOS_RPI4_EMMC2_PROBE_DIAG_H
#define ARMONIOS_RPI4_EMMC2_PROBE_DIAG_H

#include <stdint.h>

#define RPI4_EMMC2_DIAG_ARGUMENT          0x08U
#define RPI4_EMMC2_DIAG_TRANSFER_COMMAND  0x0cU
#define RPI4_EMMC2_DIAG_PRESENT_STATE     0x24U
#define RPI4_EMMC2_DIAG_HOST_POWER        0x28U
#define RPI4_EMMC2_DIAG_CLOCK_RESET       0x2cU
#define RPI4_EMMC2_DIAG_INT_STATUS        0x30U

#define RPI4_EMMC2_DIAG_CARD_PRESENT      0x00010000U
#define RPI4_EMMC2_DIAG_CARD_STABLE       0x00020000U
#define RPI4_EMMC2_DIAG_NO_COMMAND        UINT32_MAX

typedef struct {
    uintptr_t base;
    uint32_t last_command;
    uint32_t last_argument;
    uint32_t last_read_offset;
    uint32_t present_state;
    uint32_t clock_reset;
    uint32_t host_power;
    uint32_t last_nonzero_interrupt_status;
} rpi4_emmc2_probe_diag_t;

static inline uint32_t rpi4_emmc2_probe_diag_raw_read(
    const rpi4_emmc2_probe_diag_t *diag, uint32_t offset) {
    return *(volatile uint32_t *)(diag->base + offset);
}

static inline void rpi4_emmc2_probe_diag_init(
    rpi4_emmc2_probe_diag_t *diag, uintptr_t base) {
    diag->base = base;
    diag->last_command = RPI4_EMMC2_DIAG_NO_COMMAND;
    diag->last_argument = 0U;
    diag->last_read_offset = 0U;
    diag->present_state = 0U;
    diag->clock_reset = 0U;
    diag->host_power = 0U;
    diag->last_nonzero_interrupt_status = 0U;
}

static inline uint32_t rpi4_emmc2_probe_diag_read32(void *context,
                                                     uint32_t offset) {
    rpi4_emmc2_probe_diag_t *diag =
        (rpi4_emmc2_probe_diag_t *)context;
    uint32_t value = rpi4_emmc2_probe_diag_raw_read(diag, offset);

    diag->last_read_offset = offset;
    if (offset == RPI4_EMMC2_DIAG_PRESENT_STATE) {
        diag->present_state = value;
        value |= RPI4_EMMC2_DIAG_CARD_PRESENT |
                 RPI4_EMMC2_DIAG_CARD_STABLE;
    } else if (offset == RPI4_EMMC2_DIAG_CLOCK_RESET) {
        diag->clock_reset = value;
    } else if (offset == RPI4_EMMC2_DIAG_HOST_POWER) {
        diag->host_power = value;
    } else if (offset == RPI4_EMMC2_DIAG_INT_STATUS && value != 0U) {
        diag->last_nonzero_interrupt_status = value;
    }
    return value;
}

static inline void rpi4_emmc2_probe_diag_write32(void *context,
                                                  uint32_t offset,
                                                  uint32_t value) {
    rpi4_emmc2_probe_diag_t *diag =
        (rpi4_emmc2_probe_diag_t *)context;

    if (offset == RPI4_EMMC2_DIAG_ARGUMENT) {
        diag->last_argument = value;
    } else if (offset == RPI4_EMMC2_DIAG_TRANSFER_COMMAND) {
        diag->last_command = (value >> 24U) & 0x3fU;
    }

    *(volatile uint32_t *)(diag->base + offset) = value;
#if defined(__aarch64__)
    __asm__ volatile("dmb sy" ::: "memory");
#else
    __sync_synchronize();
#endif
}

static inline void rpi4_emmc2_probe_diag_refresh(
    rpi4_emmc2_probe_diag_t *diag) {
    uint32_t interrupt_status;

    diag->present_state = rpi4_emmc2_probe_diag_raw_read(
        diag, RPI4_EMMC2_DIAG_PRESENT_STATE);
    diag->clock_reset = rpi4_emmc2_probe_diag_raw_read(
        diag, RPI4_EMMC2_DIAG_CLOCK_RESET);
    diag->host_power = rpi4_emmc2_probe_diag_raw_read(
        diag, RPI4_EMMC2_DIAG_HOST_POWER);
    interrupt_status = rpi4_emmc2_probe_diag_raw_read(
        diag, RPI4_EMMC2_DIAG_INT_STATUS);
    if (interrupt_status != 0U) {
        diag->last_nonzero_interrupt_status = interrupt_status;
    }
}

#endif
