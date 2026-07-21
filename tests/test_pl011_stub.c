/*
 * Host-side stubs for platform services not exercised by the native suite.
 *
 * The real pl011.c uses aarch64 `yield` instructions in its spin loops which
 * host gcc does not understand. The runtime network wrapper also references the
 * real DHCP poll symbol, while the broad host suite tests only the virtio driver
 * and runtime mechanics. These stubs satisfy those platform edges.
 */

#include <stdint.h>
#include <stdio.h>

void uart_putc(char c) {
    fputc(c, stdout);
}

void uart_puts(const char *s) {
    while (*s != 0) {
        fputc(*s, stdout);
        s++;
    }
}

void net_poll(void) {
}
