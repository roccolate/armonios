#ifndef ARMONIOS_KERNEL_IO_SERVICE_H
#define ARMONIOS_KERNEL_IO_SERVICE_H

#include <stdint.h>

typedef struct {
    uint32_t uart_events;
    uint32_t board_input_events;
    uint32_t usb_input_events;
} kernel_io_input_poll_result_t;

/*
 * Poll the input producers that feed the shared kernel input queue.
 *
 * UART polling is optional because the timer path preserves its current
 * board/USB-only behavior, while the console and syscall paths also service
 * serial input. Counts describe events successfully reported by each producer.
 */
kernel_io_input_poll_result_t kernel_io_poll_input_sources(uint8_t include_uart);

/* Poll the network only on boards that declare BOARD_CAP_NET.
 * Returns the number of received frames drained by this call. */
uint32_t kernel_io_poll_network(void);

#endif
