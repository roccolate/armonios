#include "uart/pl011.h"

#include <stdint.h>

#define UART_DR        0x00
#define UART_FR        0x18
#define UART_IMSC      0x38
#define UART_MIS       0x40
#define UART_ICR       0x44
#define UART_FR_RXFE   (1U << 4)
#define UART_FR_TXFF   (1U << 5)
#define UART_INT_RX    (1U << 4)
#define UART_INT_RT    (1U << 6)
#define UART_RX_BUFFER_SIZE 128U

static char g_rx_buffer[UART_RX_BUFFER_SIZE];
static uint32_t g_rx_head;
static uint32_t g_rx_tail;
static uint64_t g_uart_base;

static volatile uint32_t *uart_reg(uint32_t offset) {
    return (volatile uint32_t *)(g_uart_base + offset);
}

void uart_init(uint64_t base) {
    /* QEMU virt firmware leaves PL011 usable for polling output. */
    g_uart_base = base;
    g_rx_head = 0;
    g_rx_tail = 0;
    *uart_reg(UART_ICR) = UART_INT_RX | UART_INT_RT;
}

void uart_putc(char c) {
    if (c == '\n') {
        uart_putc('\r');
    }

    while ((*uart_reg(UART_FR) & UART_FR_TXFF) != 0) {
        __asm__ volatile("yield");
    }

    *uart_reg(UART_DR) = (uint32_t)c;
}

void uart_puts(const char *s) {
    while (*s != '\0') {
        uart_putc(*s++);
    }
}

static uint32_t rx_next(uint32_t index) {
    return (index + 1U) % UART_RX_BUFFER_SIZE;
}

static void rx_push(char c) {
    uint32_t next = rx_next(g_rx_head);

    if (next == g_rx_tail) {
        return;
    }

    g_rx_buffer[g_rx_head] = c;
    g_rx_head = next;
}

void uart_enable_rx_irq(void) {
    *uart_reg(UART_IMSC) |= UART_INT_RX | UART_INT_RT;
}

void uart_irq_handler(void *context) {
    (void)context;

    uint32_t pending = *uart_reg(UART_MIS);
    if ((pending & (UART_INT_RX | UART_INT_RT)) == 0) {
        return;
    }

    while ((*uart_reg(UART_FR) & UART_FR_RXFE) == 0) {
        rx_push((char)(*uart_reg(UART_DR) & 0xffU));
    }

    *uart_reg(UART_ICR) = UART_INT_RX | UART_INT_RT;
}

int uart_getc_nonblock(void) {
    char c;

    if (g_rx_head == g_rx_tail) {
        return -1;
    }

    c = g_rx_buffer[g_rx_tail];
    g_rx_tail = rx_next(g_rx_tail);

    return (int)(uint8_t)c;
}

uint32_t uart_rx_available(void) {
    if (g_rx_head >= g_rx_tail) {
        return g_rx_head - g_rx_tail;
    }

    return UART_RX_BUFFER_SIZE - g_rx_tail + g_rx_head;
}
