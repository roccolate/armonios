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
    """QEMU_FS_TEST_LOG := $(BUILD_DIR)/qemu-fs-test.log\nQEMU_FS_TEST_TIMEOUT ?= 25s\n""",
    """QEMU_FS_TEST_LOG := $(BUILD_DIR)/qemu-fs-test.log\nQEMU_FS_TEST_TIMEOUT ?= 25s\nSDK_DIR ?= $(BUILD_DIR)/sdk\nSDK_HELLO_KLI := $(SDK_DIR)/examples/hello-console/build/HELLO.KLI\n""",
)
replace_once(
    makefile,
    """$(VIRTIO_BLK_IMG): $(MKFAT32_IMAGE) $(BUILD_DIR)/$(APPS_DIR)/shell.bin | $(BUILD_DIR)\n\t$(LOG_MKFAT32)$(MKFAT32_IMAGE) $@ $(BUILD_DIR)/$(APPS_DIR)/shell.bin\n""",
    """$(SDK_HELLO_KLI): sdk\n\t$(Q)$(MAKE) --no-print-directory -C $(SDK_DIR)/examples/hello-console \\\n\t    SDK=$(abspath $(SDK_DIR))\n\n$(VIRTIO_BLK_IMG): $(MKFAT32_IMAGE) $(BUILD_DIR)/$(APPS_DIR)/shell.bin $(SDK_HELLO_KLI) | $(BUILD_DIR)\n\t$(LOG_MKFAT32)$(MKFAT32_IMAGE) $@ \\\n\t    $(BUILD_DIR)/$(APPS_DIR)/shell.bin $(SDK_HELLO_KLI)\n""",
)

shell = Path("programs/apps/shell.c")
replace_once(
    shell,
    """}\n\nstatic void cmd_pwd(shell_state_t *s) {\n""",
    """}\n\nstatic void cmd_exec(shell_state_t *s, const char *input) {\n    shell_arg_result_t arg = parse_one_arg(input, s->arg_buf,\n                                           sizeof(s->arg_buf));\n    long pid;\n\n    if (arg == SHELL_ARG_MISSING) {\n        shell_emit(s, \"EXEC: MISSING PATH\");\n        return;\n    }\n    if (arg == SHELL_ARG_TOO_MANY) {\n        shell_emit(s, \"EXEC: TOO MANY ARGS\");\n        return;\n    }\n    if (arg == SHELL_ARG_TOO_LONG ||\n        resolve_path(s, s->arg_buf, s->path_buf, sizeof(s->path_buf)) != 0) {\n        shell_emit(s, \"EXEC: PATH TOO LONG\");\n        return;\n    }\n\n    pid = kli_spawn(s->path_buf, 0);\n    if (pid < 0) {\n        shell_emit(s, \"EXEC: FAILED\");\n        return;\n    }\n\n    s->last_spawned_pid = (int)pid;\n    shell_emit(s, \"EXEC: SPAWNED PID\");\n    kli_utoa((uint64_t)pid, s->numbuf, sizeof(s->numbuf));\n    shell_emit(s, s->numbuf);\n}\n\nstatic void cmd_pwd(shell_state_t *s) {\n""",
)
replace_once(
    shell,
    """        shell_emit(s, \"HELP PWD CD LS CAT RUN KILL EXIT\");\n""",
    """        shell_emit(s, \"HELP PWD CD LS CAT RUN EXEC KILL EXIT\");\n""",
)
replace_once(
    shell,
    """    } else if (starts_with(line, \"run \")) {\n        cmd_run(s, line + 4);\n""",
    """    } else if (starts_with(line, \"exec \")) {\n        cmd_exec(s, line + 5);\n    } else if (strcmp(line, \"exec\") == 0) {\n        cmd_exec(s, \"\");\n    } else if (starts_with(line, \"run \")) {\n        cmd_run(s, line + 4);\n""",
)

kernel = Path("kernel/kernel.c")
replace_once(
    kernel,
    """    vfs_stat_t stat;\n""",
    """    vfs_stat_t stat;\n    vfs_metadata_t external_image;\n""",
)
replace_once(
    kernel,
    """    if (fat32_mount_vfs_file(&g_fat32_fs, \"/fat/edit.txt\",\n                              \"EDIT.TXT\") == 0) {\n        uart_puts(\"FAT32 edit file: mounted\\n\");\n    } else {\n        uart_puts(\"FAT32 edit: no\\n\");\n    }\n    return 0;\n""",
    """    if (fat32_mount_vfs_file(&g_fat32_fs, \"/fat/edit.txt\",\n                              \"EDIT.TXT\") == 0) {\n        uart_puts(\"FAT32 edit file: mounted\\n\");\n    } else {\n        uart_puts(\"FAT32 edit: no\\n\");\n    }\n\n    if (vfs_metadata(\"/fat/HELLO.KLI\", &external_image) == 0 &&\n        external_image.type == VFS_FILE_TYPE_REGULAR) {\n        uart_puts(\"FAT32 external KLI bytes: \" );\n        print_hex64(external_image.size);\n        uart_puts(\"\\n\");\n    } else {\n        uart_puts(\"FAT32 external KLI: absent\\n\");\n    }\n    return 0;\n""",
)
