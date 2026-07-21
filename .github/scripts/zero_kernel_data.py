from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def replace_once(path: str, old: str, new: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, got {count}: {old!r}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "kernel/sched/sched.c",
    "static uint32_t g_quantum_ticks = 1;\n"
    "static uint32_t g_ticks_left = 1;\n",
    "static uint32_t g_quantum_ticks;\n"
    "static uint32_t g_ticks_left;\n",
)
replace_once(
    "kernel/sched/sched.c",
    "static uint32_t g_next_pid = 1;\n",
    "static uint32_t g_next_pid;\n",
)

replace_once(
    "kernel/mm/kheap.c",
    "static uint64_t g_next_arena_id = 1;\n",
    "static uint64_t g_next_arena_id;\n",
)
replace_once(
    "kernel/mm/kheap.c",
    "    if (g_heap_head != NULL) {\n"
    "        return;\n"
    "    }\n\n"
    "    (void)extend_heap(1);\n",
    "    if (g_heap_head != NULL) {\n"
    "        return;\n"
    "    }\n\n"
    "    g_next_arena_id = 1;\n"
    "    (void)extend_heap(1);\n",
)

replace_once(
    "kernel/net/dhcp.c",
    "static uint32_t g_dhcp_xid = 0x12345678;\n",
    "static uint32_t g_dhcp_xid;\n",
)

replace_once(
    "kernel/panel_boot.c",
    "static uint32_t g_next_spawn_pid = PANEL_BOOT_PID_BASE + 1U;\n",
    "static uint32_t g_next_spawn_pid;\n",
)
replace_once(
    "kernel/panel_boot.c",
    "    uint32_t slot;\n\n"
    "    (void)process_reclaim_zombies();\n"
    "    panel = process_alloc(PANEL_BOOT_PID_BASE, PANEL_BOOT_APP);\n",
    "    uint32_t slot;\n\n"
    "    if (g_next_spawn_pid == 0U) {\n"
    "        g_next_spawn_pid = PANEL_BOOT_PID_BASE + 1U;\n"
    "    }\n"
    "    (void)process_reclaim_zombies();\n"
    "    panel = process_alloc(PANEL_BOOT_PID_BASE, PANEL_BOOT_APP);\n",
)

replace_once(
    "Makefile",
    "size: $(KERNEL_ELF) $(KERNEL_BIN)\n"
    "\t$(LOG_SIZE)$(SIZE) $(KERNEL_ELF)\n"
    "\t@bytes=$$(wc -c < $(KERNEL_BIN)); \\\n"
    "\tprintf \"kernel.bin: %s bytes (limit: $(KERNEL_SIZE_LIMIT))\\n\" \"$$bytes\"; \\\n"
    "\ttest \"$$bytes\" -lt $(KERNEL_SIZE_LIMIT)\n",
    "size: $(KERNEL_ELF) $(KERNEL_BIN)\n"
    "\t$(LOG_SIZE)$(SIZE) $(KERNEL_ELF)\n"
    "\t@data_bytes=$$($(SIZE) $(KERNEL_ELF) | awk 'NR == 2 { print $$2 }'); \\\n"
    "\tprintf \"kernel initialized data: %s bytes (required: 0)\\n\" \"$$data_bytes\"; \\\n"
    "\ttest \"$$data_bytes\" -eq 0\n"
    "\t@bytes=$$(wc -c < $(KERNEL_BIN)); \\\n"
    "\tprintf \"kernel.bin: %s bytes (limit: $(KERNEL_SIZE_LIMIT))\\n\" \"$$bytes\"; \\\n"
    "\ttest \"$$bytes\" -lt $(KERNEL_SIZE_LIMIT)\n",
)

replace_once(
    "docs/ARCHITECTURE.md",
    "- kernel text is mapped RX, rodata R/NX, data+bss+stack RW/NX, and remaining RAM RW/NX;\n",
    "- kernel text is mapped RX, rodata R/NX, data+bss+stack RW/NX, and remaining RAM RW/NX;\n"
    "- mutable kernel globals use zero-initialized BSS and subsystem init functions establish non-zero defaults, keeping the loadable `.data` section empty while preserving the page-aligned W^X boundary;\n",
)
replace_once(
    "docs/CURRENT_STATE.md",
    "| PMM/VMM/heap | IMPLEMENTED; HOST-VERIFIED | Allocation, mapping, rollback, cleanup, and heap tests | PMM manages at most 128 MiB; kernel RAM mappings are now W^X (RISK-008: text RX, rodata R/NX, data+bss+stack RW+NX, MMIO device+NX, remaining RAM RW+NX). |\n",
    "| PMM/VMM/heap | IMPLEMENTED; HOST-VERIFIED | Allocation, mapping, rollback, cleanup, heap tests, and a zero-initialized-data size gate | PMM manages at most 128 MiB; kernel RAM mappings are W^X, and mutable non-zero defaults are established by subsystem init functions so the loadable `.data` section remains empty. |\n",
)

print("zero-data optimization applied")
