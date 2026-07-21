#!/usr/bin/env python3

from pathlib import Path

path = Path("kernel/kernel.c")
text = path.read_text()

old_open = """void kernel_main(uint64_t dtb_addr);

static fat32_fs_t g_fat32_fs;
"""
new_open = """void kernel_main(uint64_t dtb_addr);

#if defined(ARMONIOS_RPI4_EMMC2_PROBE)
void kernel_main(uint64_t dtb_addr) {
    (void)dtb_addr;
    board_early_init();
    for (;;) {
        __asm__ volatile("wfe");
    }
}
#else
static fat32_fs_t g_fat32_fs;
"""
if text.count(old_open) != 1:
    raise SystemExit("expected one kernel implementation opening")
text = text.replace(old_open, new_open, 1)

old_inner = """#if defined(ARMONIOS_RPI4_EMMC2_PROBE)
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
if text.count(old_inner) != 1:
    raise SystemExit("expected one nested probe kernel_main")
text = text.replace(old_inner, "void kernel_main(uint64_t dtb_addr) {\n", 1)

path.write_text(text)
Path(__file__).unlink()
