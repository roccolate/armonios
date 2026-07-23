CC=gcc
OBJCOPY=objcopy
CFLAGS=-I.. -I../drivers -I../programs -Iunity -g -O0 -Wall -Wextra -DARMONIOS_TEST
BUILD_DIR := ../build
APPS_DIR  := programs/apps

# Shipping app .bin files live in the main build tree. They are
# pure-byte images with a known layout (KLI1 header + code + rodata).
# Convert each .bin into a host-architecture .o with a small section
# of raw bytes; the test then reads the .bin back through its
# embedded symbol to assert image_size covers the whole blob. The
# production kernel does the same with aarch64 objcopy, so this
# test catches the same shape of bug in the BUILD process.
APP_BLOB_BINS  := $(BUILD_DIR)/$(APPS_DIR)/shell.bin \
                   $(BUILD_DIR)/$(APPS_DIR)/editor.bin \
                   $(BUILD_DIR)/$(APPS_DIR)/files.bin \
                   $(BUILD_DIR)/$(APPS_DIR)/monitor.bin \
                   $(BUILD_DIR)/$(APPS_DIR)/clock.bin \
                   $(BUILD_DIR)/$(APPS_DIR)/panel.bin \
                   $(BUILD_DIR)/$(APPS_DIR)/control.bin
APP_BLOB_OBJS  := $(patsubst %.bin,%.host_blob.o,$(APP_BLOB_BINS))

SRCS=unity/unity.c main.c \
     test_pmm.c test_kheap.c test_vmm.c test_process.c \
     test_init_status.c \
     test_boot_program.c test_bootfs.c test_user_image.c test_user_vm.c \
     test_user_image_runtime.c \
     test_panel_boot_argv.c \
     test_panel_boot_recovery.c \
     test_irq.c test_sched.c test_vfs.c test_vfs_mount_dispatch.c \
     test_vfs_fat32_invalidation.c test_fat32_vfs_registration.c \
     test_tmpfs.c test_fat32.c test_dhcp_options.c \
     test_ipc.c test_font.c \
     test_gui.c test_fb.c test_dtb.c test_virtio_blk.c test_virtio_net.c \
     test_input.c test_virtio_input.c \
     test_hid.c test_pci.c test_hid_driver.c test_usb_core.c \
     test_pci_assign.c test_hid_state.c test_hid_init.c \
     test_syscall_abi.c test_syscall_helpers.c test_syscall_vfs_user_copy.c \
     test_syscall_boundary_user_copy.c test_syscall_gui_info_user_copy.c \
     test_process_isolation.c test_window_abi.c \
     ../kernel/mm/pmm.c ../kernel/mm/kheap.c ../kernel/mm/vmm.c \
     ../kernel/boot_program.c ../kernel/bootfs.c \
     ../kernel/panel_boot_argv.c ../kernel/panel_boot_recovery.c \
     ../kernel/fat32.c ../kernel/fat32_directory.c ../kernel/fat32_vfs.c \
     ../kernel/net/dhcp_options.c \
     ../kernel/font.c ../kernel/gui_backing.c \
     ../kernel/gui_cursor.c ../kernel/gui_damage.c \
     ../kernel/gui_events.c ../kernel/gui_input.c ../kernel/gui_pool.c \
     ../kernel/gui_compositor.c \
     ../kernel/init_status.c ../kernel/print.c \
     ../kernel/ipc.c ../kernel/irq.c ../kernel/process.c \
     ../kernel/sched/sched.c \
     ../kernel/tmpfs.c ../kernel/syscall_helpers.c ../kernel/syscall_ipc.c \
     ../kernel/syscall_gui.c ../kernel/syscall_info.c ../kernel/syscall_vfs.c \
     ../kernel/kstring.c \
     ../kernel/user_image.c ../kernel/user_vm.c ../kernel/vfs.c \
     ../kernel/dtb.c ../kernel/dtb_framebuffer.c \
     ../drivers/fb/fb.c ../drivers/storage/virtio_blk.c \
     ../drivers/net/virtio_net.c \
     ../drivers/input/input.c ../drivers/input/keymap.c \
     ../drivers/input/virtio_input.c \
     ../drivers/usb/hid.c ../drivers/usb/hid_driver.c \
     ../drivers/usb/xhci.c ../drivers/usb/usb_core.c \
     ../drivers/pci/pci.c test_pl011_stub.c test_panel_boot_recovery_stub.c \
     $(APP_BLOB_OBJS)

TARGET=run_tests
LIBKARM_ARENA_TARGET=run_libkarm_arena_tests
LIBKARM_ARENA_SRCS=unity/unity.c test_libkarm_arena.c \
                   ../programs/libkarm/arena.c \
                   ../programs/libkarm/arena_map.c
LIBKARM_BUFFER_TARGET=run_libkarm_buffer_tests
LIBKARM_BUFFER_SRCS=unity/unity.c test_libkarm_buffer.c \
                    ../programs/libkarm/arena.c \
                    ../programs/libkarm/buffer.c \
                    ../programs/libkarm/string.c

all: $(TARGET) $(LIBKARM_ARENA_TARGET) $(LIBKARM_BUFFER_TARGET)

$(TARGET): $(SRCS) $(APP_BLOB_OBJS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

$(LIBKARM_ARENA_TARGET): $(LIBKARM_ARENA_SRCS)
	$(CC) $(CFLAGS) $(LIBKARM_ARENA_SRCS) -o $@

$(LIBKARM_BUFFER_TARGET): $(LIBKARM_BUFFER_SRCS)
	$(CC) $(CFLAGS) $(LIBKARM_BUFFER_SRCS) -o $@

%.host_blob.o: %.bin
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
	    --rename-section .data=.app_host_blob,alloc,load,readonly,data,contents \
	    --add-section .note.GNU-stack=/dev/null \
	    --set-section-flags .note.GNU-stack=readonly \
	    $< $@

.PHONY: test

test: all
	./$(TARGET)
	./$(LIBKARM_ARENA_TARGET)
	./$(LIBKARM_BUFFER_TARGET)

.PHONY: clean

clean:
	rm -f $(TARGET) $(LIBKARM_ARENA_TARGET) $(LIBKARM_BUFFER_TARGET) *.o
