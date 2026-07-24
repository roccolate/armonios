#!/usr/bin/env python3

from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}")
    path.write_text(text.replace(old, new, 1))


makefile = Path("Makefile")
replace_once(
    makefile,
    """    $(BUILD_DIR)/kernel/boot_program.o \\\n    $(BUILD_DIR)/kernel/bootfs.o \\\n""",
    """    $(BUILD_DIR)/kernel/app_image_source.o \\\n    $(BUILD_DIR)/kernel/boot_program.o \\\n    $(BUILD_DIR)/kernel/bootfs.o \\\n""",
)

panel = Path("kernel/panel_boot.c")
replace_once(
    panel,
    """#include "kernel/boot_program.h"\n#include "kernel/aarch64_state.h"\n""",
    """#include "kernel/boot_program.h"\n#include "kernel/aarch64_state.h"\n#include "kernel/app_image_source.h"\n""",
)
replace_once(
    panel,
    """static int load_named_image(const char *name, user_image_t *image,\n                            uint32_t slot, uint32_t entry_index,\n                            panel_user_storage_t *storage) {\n    uint64_t image_paddr;\n\n    if (storage == 0 || storage->image_paddr != 0) {\n        return -1;\n    }\n\n    image_paddr = pmm_alloc_pages(KERNEL_USER_IMAGE_SLOT_PAGES);\n    if (image_paddr == 0) {\n        return -1;\n    }\n    zero_memory(image_paddr, KERNEL_USER_IMAGE_SLOT_SIZE);\n\n    if (user_image_load_bootfs_flat(image, name, name,\n                                    image_paddr,\n                                    KERNEL_USER_IMAGE_SLOT_SIZE,\n                                    entry_index) != 0) {\n        free_image_pages(image_paddr);\n        return -1;\n    }\n\n    /*\n     * The image region maps the full slot, not only the declared KLI1 byte\n     * count, because the process owns and later frees the whole PMM allocation.\n     * The unused tail was zeroed above before the KLI1 payload was copied.\n     */\n    storage->image_paddr = image_paddr;\n    image->base = panel_image_vaddr(slot);\n    image->size = KERNEL_USER_IMAGE_SLOT_SIZE;\n    return 0;\n}\n""",
    """static int load_image(const char *name, const char *bootfs_name,\n                      const char *vfs_path, user_image_t *image,\n                      uint32_t slot, uint32_t entry_index,\n                      panel_user_storage_t *storage) {\n    uint64_t image_paddr;\n    int status;\n\n    if (name == 0 || image == 0 || storage == 0 ||\n        storage->image_paddr != 0 ||\n        ((bootfs_name == 0) == (vfs_path == 0))) {\n        return -1;\n    }\n\n    image_paddr = pmm_alloc_pages(KERNEL_USER_IMAGE_SLOT_PAGES);\n    if (image_paddr == 0) {\n        return -1;\n    }\n    zero_memory(image_paddr, KERNEL_USER_IMAGE_SLOT_SIZE);\n\n    if (bootfs_name != 0) {\n        status = user_image_load_bootfs_flat(\n            image, name, bootfs_name, image_paddr,\n            KERNEL_USER_IMAGE_SLOT_SIZE, entry_index);\n    } else {\n        status = user_image_load_vfs_flat(\n            image, name, vfs_path, image_paddr,\n            KERNEL_USER_IMAGE_SLOT_SIZE, entry_index);\n    }\n    if (status != 0) {\n        free_image_pages(image_paddr);\n        return -1;\n    }\n\n    /*\n     * The image region maps the full slot, not only the declared KLI1 byte\n     * count, because the process owns and later frees the whole PMM allocation.\n     * The unused tail was zeroed above before the KLI1 payload was copied.\n     */\n    storage->image_paddr = image_paddr;\n    image->base = panel_image_vaddr(slot);\n    image->size = KERNEL_USER_IMAGE_SLOT_SIZE;\n    return 0;\n}\n""",
)
replace_once(
    panel,
    """    uint32_t slot;\n    const char *app_name;\n    size_t name_len;\n    uint64_t argv_vaddr = 0;\n\n    if (path == 0 || g_spawn_memory_size == 0) {\n        return -1;\n    }\n\n    app_name = vfs_strip_prefix(path, "/armonios/");\n    if (app_name == 0) {\n        return -1;\n    }\n\n    name_len = 0;\n    while (app_name[name_len] != '\\0') {\n        name_len++;\n    }\n    if (name_len == 0 || name_len >= 32) {\n        return -1;\n    }\n""",
    """    uint32_t slot;\n    app_image_source_t source;\n    uint64_t argv_vaddr = 0;\n\n    if (g_spawn_memory_size == 0 ||\n        app_image_source_resolve(path, &source) != 0) {\n        return -1;\n    }\n""",
)
replace_once(
    panel,
    """    process = process_alloc(g_next_spawn_pid++, app_name);\n""",
    """    process = process_alloc(g_next_spawn_pid++, source.name);\n""",
)
replace_once(
    panel,
    """    if (load_named_image(app_name, &image, slot, entry_index, &storage) != 0) {\n""",
    """    if (load_image(\n            source.name,\n            source.kind == APP_IMAGE_SOURCE_BOOTFS ? source.path : 0,\n            source.kind == APP_IMAGE_SOURCE_VFS ? source.path : 0,\n            &image, slot, entry_index, &storage) != 0) {\n""",
)
replace_once(
    panel,
    """    if (load_named_image(PANEL_BOOT_APP, &panel_image, slot, 0,\n                         &storage) != 0) {\n""",
    """    if (load_image(PANEL_BOOT_APP, PANEL_BOOT_APP, 0, &panel_image,\n                   slot, 0, &storage) != 0) {\n""",
)
