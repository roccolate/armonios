#ifndef ARMONIOS_KERNEL_IO_SERVICE_H
#define ARMONIOS_KERNEL_IO_SERVICE_H

#include <stdint.h>

/*
 * Poll the input producers that feed the shared kernel input queue.
 *
 * UART polling is optional because the timer path must preserve its current
 * board/USB-only behavior, while the console and syscall paths also service
 * serial input. This service owns board gates; callers own event routing.
 */
void kernel_io_poll_input_sources(uint8_t include_uart);

/* Poll the network only on boards that declare BOARD_CAP_NET. */
void kernel_io_poll_network(void);

#endif
