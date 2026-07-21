#!/usr/bin/env python3

from pathlib import Path

path = Path("kernel/panel_boot.c")
text = path.read_text()

if text.count("const uint64_t *argv_ptr") != 2:
    raise SystemExit("expected two argv pointer parameter declarations")
text = text.replace("const uint64_t *argv_ptr", "const panel_boot_argv_t *argv")

for old, new in [
    ("argv_ptr, argc, out_argv_vaddr", "argv, argc, out_argv_vaddr"),
    ("slot, argv_ptr, argc", "slot, argv, argc"),
]:
    if text.count(old) != 1:
        raise SystemExit(f"expected one match for {old!r}")
    text = text.replace(old, new, 1)

path.write_text(text)
Path(__file__).unlink()
