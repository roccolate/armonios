#!/usr/bin/env python3

from pathlib import Path

path = Path("kernel/kernel.c")
text = path.read_text()
needle = "void kernel_main(uint64_t dtb_addr) {\n"
if text.count(needle) != 1:
    raise SystemExit("expected one kernel_main definition")
replacement = """#if defined(ARMONIOS_RPI4_EMMC2_PROBE)
void kernel_main(uint64_t dtb_addr) {
    (void)dtb_addr;
    board_early_init();
    for (;;) {
        __asm__ volatile("wfe");
    }
}
#else
void kernel_main(uint64_t dtb_addr) {
"""
text = text.replace(needle, replacement, 1)
if not text.endswith("}\n"):
    raise SystemExit("unexpected kernel.c ending")
text += "#endif\n"
path.write_text(text)
Path(__file__).unlink()
