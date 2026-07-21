#include "firmware/rpi_mailbox.h"

#include <stddef.h>
#include <stdint.h>

#define RPI_MAILBOX_READ               0x00U
#define RPI_MAILBOX_STATUS             0x18U
#define RPI_MAILBOX_WRITE              0x20U

#define RPI_MAILBOX_STATUS_FULL        0x80000000U
#define RPI_MAILBOX_STATUS_EMPTY       0x40000000U
#define RPI_MAILBOX_CHANNEL_PROPERTY   8U
#define RPI_MAILBOX_CHANNEL_MASK       0x0fU
#define RPI_MAILBOX_ADDRESS_MASK       0xfffffff0U
#define RPI_MAILBOX_POLL_LIMIT         1000000U

#define RPI_FIRMWARE_STATUS_REQUEST    0x00000000U
#define RPI_FIRMWARE_STATUS_SUCCESS    0x80000000U
#define RPI_FIRMWARE_RESPONSE_BIT      0x80000000U
#define RPI_FIRMWARE_TAG_GET_CLOCK_RATE 0x00030002U
#define RPI_FIRMWARE_TAG_END           0x00000000U

static uint32_t g_clock_message[8] __attribute__((aligned(16)));

static uint32_t rpi_mailbox_mmio_read32(void *context, uint32_t offset) {
    uintptr_t base = (uintptr_t)context;
    return *(volatile uint32_t *)(base + offset);
}

static void rpi_mailbox_mmio_write32(void *context, uint32_t offset,
                                     uint32_t value) {
    uintptr_t base = (uintptr_t)context;
    *(volatile uint32_t *)(base + offset) = value;
}

static void rpi_mailbox_default_barrier(void *context) {
    (void)context;
#if defined(__aarch64__)
    __asm__ volatile("dmb sy" ::: "memory");
#else
    __sync_synchronize();
#endif
}

static int rpi_mailbox_wait_status(const rpi_mailbox_io_t *io,
                                   uint32_t mask, uint32_t expected) {
    for (uint32_t attempt = 0; attempt < RPI_MAILBOX_POLL_LIMIT; attempt++) {
        if ((io->read32(io->context, RPI_MAILBOX_STATUS) & mask) == expected) {
            return RPI_MAILBOX_OK;
        }
    }
    return RPI_MAILBOX_ERR_TIMEOUT;
}

static int rpi_mailbox_transaction(const rpi_mailbox_io_t *io,
                                   uint32_t message_address) {
    uint32_t request;

    if (io == NULL || io->read32 == NULL || io->write32 == NULL ||
        io->barrier == NULL || message_address == 0U ||
        (message_address & RPI_MAILBOX_CHANNEL_MASK) != 0U) {
        return RPI_MAILBOX_ERR_INVAL;
    }

    request = (message_address & RPI_MAILBOX_ADDRESS_MASK) |
              RPI_MAILBOX_CHANNEL_PROPERTY;
    if (rpi_mailbox_wait_status(io, RPI_MAILBOX_STATUS_FULL, 0U) !=
        RPI_MAILBOX_OK) {
        return RPI_MAILBOX_ERR_TIMEOUT;
    }

    io->barrier(io->context);
    io->write32(io->context, RPI_MAILBOX_WRITE, request);

    for (uint32_t attempt = 0; attempt < RPI_MAILBOX_POLL_LIMIT; attempt++) {
        uint32_t response;

        if ((io->read32(io->context, RPI_MAILBOX_STATUS) &
             RPI_MAILBOX_STATUS_EMPTY) != 0U) {
            continue;
        }
        response = io->read32(io->context, RPI_MAILBOX_READ);
        if (response == request) {
            io->barrier(io->context);
            return RPI_MAILBOX_OK;
        }
    }

    return RPI_MAILBOX_ERR_TIMEOUT;
}

int rpi_mailbox_get_clock_rate_with_io(const rpi_mailbox_io_t *io,
                                       uint32_t message_address,
                                       uint32_t message[8],
                                       uint32_t clock_id,
                                       uint32_t *rate_hz) {
    int result;

    if (message == NULL || rate_hz == NULL || clock_id == 0U ||
        ((uintptr_t)message & RPI_MAILBOX_CHANNEL_MASK) != 0U) {
        return RPI_MAILBOX_ERR_INVAL;
    }

    message[0] = 8U * sizeof(uint32_t);
    message[1] = RPI_FIRMWARE_STATUS_REQUEST;
    message[2] = RPI_FIRMWARE_TAG_GET_CLOCK_RATE;
    message[3] = 2U * sizeof(uint32_t);
    message[4] = 0U;
    message[5] = clock_id;
    message[6] = 0U;
    message[7] = RPI_FIRMWARE_TAG_END;

    result = rpi_mailbox_transaction(io, message_address);
    if (result != RPI_MAILBOX_OK) {
        return result;
    }
    if (message[1] != RPI_FIRMWARE_STATUS_SUCCESS ||
        (message[4] & RPI_FIRMWARE_RESPONSE_BIT) == 0U ||
        (message[4] & ~RPI_FIRMWARE_RESPONSE_BIT) <
            2U * sizeof(uint32_t) ||
        message[5] != clock_id || message[6] == 0U) {
        return RPI_MAILBOX_ERR_FIRMWARE;
    }

    *rate_hz = message[6];
    return RPI_MAILBOX_OK;
}

int rpi_mailbox_get_clock_rate(uint64_t mailbox_base, uint32_t clock_id,
                               uint32_t *rate_hz) {
    rpi_mailbox_io_t io;
    uintptr_t message_address = (uintptr_t)g_clock_message;

    if (mailbox_base == 0U || message_address > UINT32_MAX) {
        return RPI_MAILBOX_ERR_INVAL;
    }

    io.read32 = rpi_mailbox_mmio_read32;
    io.write32 = rpi_mailbox_mmio_write32;
    io.barrier = rpi_mailbox_default_barrier;
    io.context = (void *)(uintptr_t)mailbox_base;
    return rpi_mailbox_get_clock_rate_with_io(
        &io, (uint32_t)message_address, g_clock_message, clock_id, rate_hz);
}
