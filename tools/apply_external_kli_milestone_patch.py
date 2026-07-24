#!/usr/bin/env python3

# One-shot exact replacement script. The branch-local workflow removes this
# file after the source migration is committed. The draft PR keeps the branch
# registered with Actions while this migration runs.

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
    """QEMU_FS_TEST_LOG := $(BUILD_DIR)/qemu-fs-test.log
QEMU_FS_TEST_TIMEOUT ?= 25s
""",
    """QEMU_FS_TEST_LOG := $(BUILD_DIR)/qemu-fs-test.log
QEMU_FS_TEST_TIMEOUT ?= 25s
SDK_DIR ?= $(BUILD_DIR)/sdk
SDK_HELLO_KLI := $(SDK_DIR)/examples/hello-console/build/HELLO.KLI
""",
)
replace_once(
    makefile,
    """$(VIRTIO_BLK_IMG): $(MKFAT32_IMAGE) $(BUILD_DIR)/$(APPS_DIR)/shell.bin | $(BUILD_DIR)
	$(LOG_MKFAT32)$(MKFAT32_IMAGE) $@ $(BUILD_DIR)/$(APPS_DIR)/shell.bin
""",
    """$(SDK_HELLO_KLI): sdk
	$(Q)$(MAKE) --no-print-directory -C $(SDK_DIR)/examples/hello-console \
	    SDK=$(abspath $(SDK_DIR))

$(VIRTIO_BLK_IMG): $(MKFAT32_IMAGE) $(BUILD_DIR)/$(APPS_DIR)/shell.bin $(SDK_HELLO_KLI) | $(BUILD_DIR)
	$(LOG_MKFAT32)$(MKFAT32_IMAGE) $@ \
	    $(BUILD_DIR)/$(APPS_DIR)/shell.bin $(SDK_HELLO_KLI)
""",
)

shell = Path("programs/apps/shell.c")
replace_once(
    shell,
    """}

static void cmd_pwd(shell_state_t *s) {
""",
    """}

static void cmd_exec(shell_state_t *s, const char *input) {
    shell_arg_result_t arg = parse_one_arg(input, s->arg_buf,
                                           sizeof(s->arg_buf));
    long pid;

    if (arg == SHELL_ARG_MISSING) {
        shell_emit(s, "EXEC: MISSING PATH");
        return;
    }
    if (arg == SHELL_ARG_TOO_MANY) {
        shell_emit(s, "EXEC: TOO MANY ARGS");
        return;
    }
    if (arg == SHELL_ARG_TOO_LONG ||
        resolve_path(s, s->arg_buf, s->path_buf, sizeof(s->path_buf)) != 0) {
        shell_emit(s, "EXEC: PATH TOO LONG");
        return;
    }

    pid = kli_spawn(s->path_buf, 0);
    if (pid < 0) {
        shell_emit(s, "EXEC: FAILED");
        return;
    }

    s->last_spawned_pid = (int)pid;
    shell_emit(s, "EXEC: SPAWNED PID");
    kli_utoa((uint64_t)pid, s->numbuf, sizeof(s->numbuf));
    shell_emit(s, s->numbuf);
}

static void cmd_pwd(shell_state_t *s) {
""",
)
replace_once(
    shell,
    """        shell_emit(s, "HELP PWD CD LS CAT RUN KILL EXIT");
""",
    """        shell_emit(s, "HELP PWD CD LS CAT RUN EXEC KILL EXIT");
""",
)
replace_once(
    shell,
    """    } else if (starts_with(line, "run ")) {
        cmd_run(s, line + 4);
""",
    """    } else if (starts_with(line, "exec ")) {
        cmd_exec(s, line + 5);
    } else if (strcmp(line, "exec") == 0) {
        cmd_exec(s, "");
    } else if (starts_with(line, "run ")) {
        cmd_run(s, line + 4);
""",
)

kernel = Path("kernel/kernel.c")
replace_once(
    kernel,
    """    vfs_stat_t stat;
""",
    """    vfs_stat_t stat;
    vfs_metadata_t external_image;
""",
)
replace_once(
    kernel,
    """    if (fat32_mount_vfs_file(&g_fat32_fs, "/fat/edit.txt",
                              "EDIT.TXT") == 0) {
        uart_puts("FAT32 edit file: mounted\n");
    } else {
        uart_puts("FAT32 edit: no\n");
    }
    return 0;
""",
    """    if (fat32_mount_vfs_file(&g_fat32_fs, "/fat/edit.txt",
                              "EDIT.TXT") == 0) {
        uart_puts("FAT32 edit file: mounted\n");
    } else {
        uart_puts("FAT32 edit: no\n");
    }

    if (vfs_metadata("/fat/HELLO.KLI", &external_image) == 0 &&
        external_image.type == VFS_FILE_TYPE_REGULAR) {
        uart_puts("FAT32 external KLI bytes: ");
        print_hex64(external_image.size);
        uart_puts("\n");
    } else {
        uart_puts("FAT32 external KLI: absent\n");
    }
    return 0;
""",
)
