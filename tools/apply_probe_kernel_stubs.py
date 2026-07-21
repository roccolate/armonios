#!/usr/bin/env python3

from pathlib import Path

path = Path("kernel/kernel.c")
text = path.read_text()
old = """#if defined(ARMONIOS_RPI4_EMMC2_PROBE)
void kernel_main(uint64_t dtb_addr) {
"""
new = """#if defined(ARMONIOS_RPI4_EMMC2_PROBE)
void kernel_io_poll_input_sources(uint8_t include_uart) {
    (void)include_uart;
}

void kernel_io_poll_network(void) {
}

void kernel_on_timer_tick(void) {
}

void kernel_main(uint64_t dtb_addr) {
"""
if text.count(old) != 1:
    raise SystemExit("expected one probe kernel opening")
path.write_text(text.replace(old, new, 1))
Path(__file__).unlink()
