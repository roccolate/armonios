#!/usr/bin/env python3

from pathlib import Path

path = Path("kernel/panel_boot.c")
text = path.read_text()

replacements = [
    (
        """static int place_argv_on_stack(uint64_t stack_paddr, uint32_t slot,
                               const uint64_t *argv_ptr,
                               uint32_t argc, uint64_t *out_argv_vaddr) {
""",
        """static int place_argv_on_stack(uint64_t stack_paddr, uint32_t slot,
                               const panel_boot_argv_t *argv,
                               uint32_t argc, uint64_t *out_argv_vaddr) {
""",
    ),
    (
        """                                           argv_ptr, argc, out_argv_vaddr);
""",
        """                                           argv, argc, out_argv_vaddr);
""",
    ),
    (
        """int app_spawn_vfs(const char *path, uint32_t entry_index,
                  const uint64_t *argv_ptr, uint32_t argc) {
""",
        """int app_spawn_vfs(const char *path, uint32_t entry_index,
                  const panel_boot_argv_t *argv, uint32_t argc) {
""",
    ),
    (
        """    if (place_argv_on_stack(storage.stack_paddr, slot, argv_ptr, argc,
                             &argv_vaddr) != 0) {
""",
        """    if (place_argv_on_stack(storage.stack_paddr, slot, argv, argc,
                             &argv_vaddr) != 0) {
""",
    ),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected one match, found {count}: {old!r}")
    text = text.replace(old, new, 1)

path.write_text(text)
Path(__file__).unlink()
