#ifndef ARMONIOS_DRIVERS_FIRMWARE_RPI_MAILBOX_H
#define ARMONIOS_DRIVERS_FIRMWARE_RPI_MAILBOX_H

#include <stdint.h>

#define RPI_MAILBOX_OK                  0
#define RPI_MAILBOX_ERR_INVAL          -1
#define RPI_MAILBOX_ERR_TIMEOUT        -2
#define RPI_MAILBOX_ERR_FIRMWARE       -3

#define RPI_FIRMWARE_CLOCK_EMMC2       12U

typedef uint32_t (*rpi_mailbox_read32_fn_t)(void *context,
                                             uint32_t offset);
typedef void (*rpi_mailbox_write32_fn_t)(void *context, uint32_t offset,
                                         uint32_t value);
typedef void (*rpi_mailbox_barrier_fn_t)(void *context);

typedef struct {
    rpi_mailbox_read32_fn_t read32;
    rpi_mailbox_write32_fn_t write32;
    rpi_mailbox_barrier_fn_t barrier;
    void *context;
} rpi_mailbox_io_t;

/*
 * The firmware consumes a VideoCore/DMA bus address, not an ARM physical
 * address. bus_alias is the board-provided translation base from its
 * dma-ranges contract (0xc0000000 on BCM2711 for the first 1 GiB of RAM).
 *
 * The non-test wrapper is an early-boot interface: its static message buffer
 * must still be identity-addressed and coherent when this function is called.
 */
int rpi_mailbox_get_clock_rate_with_io(const rpi_mailbox_io_t *io,
                                       uintptr_t message_arm_address,
                                       uint32_t bus_alias,
                                       uint32_t message[8],
                                       uint32_t clock_id,
                                       uint32_t *rate_hz);
int rpi_mailbox_get_clock_rate(uint64_t mailbox_base, uint32_t bus_alias,
                               uint32_t clock_id, uint32_t *rate_hz);

#endif
