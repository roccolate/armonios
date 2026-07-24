# ArmoniOS top-level build policy and post-Makefile composition.
#
# GNU make prefers this file over Makefile. The main build remains in Makefile;
# cross-cutting foundation objects are appended here so every board links the
# same storage and ABI contracts without duplicating board conditionals.

# 128 KiB gives the kernel, embedded applications, and desktop foundations
# enough development headroom while retaining a hard, measurable size gate.
KERNEL_SIZE_LIMIT ?= 131072

include Makefile

# Public ABI and userland headers are rooted at include/. Keep the historical
# root include path during migration, but also compile the in-tree kernel,
# applications, and assembly with the same installable paths exposed by the SDK.
CFLAGS += -I include
USERLAND_CFLAGS += -I include
USERLAND_ASFLAGS += -I include

FOUNDATION_OBJS := \
    $(BUILD_DIR)/drivers/storage/block_device.o \
    $(BUILD_DIR)/kernel/fat32_device.o \
    $(BUILD_DIR)/kernel/fat32_directory.o \
    $(BUILD_DIR)/kernel/vfs_metadata.o \
    $(BUILD_DIR)/kernel/syscall_vfs_metadata.o \
    $(BUILD_DIR)/kernel/vfs_fsinfo.o \
    $(BUILD_DIR)/kernel/syscall_vfs_fsinfo.o

# RPi4 diagnostic builds already add mbr.o through STORAGE_DEV. Every other
# production build needs it because kernel storage can open bounded MBR
# partition views.
ifeq ($(filter $(BUILD_DIR)/drivers/storage/mbr.o,$(OBJS)),)
FOUNDATION_OBJS += $(BUILD_DIR)/drivers/storage/mbr.o
endif

OBJS += $(FOUNDATION_OBJS)
DEPS += $(FOUNDATION_OBJS:.o=.d)

# The original dependency include was parsed inside Makefile before the objects
# above were appended.
-include $(FOUNDATION_OBJS:.o=.d)

# The original kernel target was parsed before the appended object list. Add the
# prerequisites explicitly; its deferred link recipe sees the updated OBJS.
$(KERNEL_ELF): $(FOUNDATION_OBJS)

# Minimal console SDK. libarmdesk is intentionally excluded until its compiled
# library and public object model are promoted in later independent cuts.
SDK_DIR ?= $(BUILD_DIR)/sdk
SDK_KLI1_HEADER_OBJ := $(BUILD_DIR)/$(APPS_DIR)/kli1_header.o
SDK_KLI1_END_OBJ := $(BUILD_DIR)/$(APPS_DIR)/kli1_end.o
SDK_HELLO_DIR := $(SDK_DIR)/examples/hello-console
SDK_HELLO_KLI := $(SDK_HELLO_DIR)/build/HELLO.KLI
EXTERNAL_KLI_IMG ?= $(BUILD_DIR)/external-kli.img

.PHONY: sdk external-kli-image qemu-external-kli

sdk: libkarm $(SDK_KLI1_HEADER_OBJ) $(SDK_KLI1_END_OBJ)
	@bash tools/build_sdk.sh "$(BUILD_DIR)" "$(SDK_DIR)"

$(SDK_HELLO_KLI): sdk
	$(Q)$(MAKE) --no-print-directory -C $(SDK_HELLO_DIR) \
	    SDK=$(abspath $(SDK_DIR))

$(EXTERNAL_KLI_IMG): $(MKFAT32_IMAGE) \
                     $(BUILD_DIR)/$(APPS_DIR)/shell.bin \
                     $(SDK_HELLO_KLI) | $(BUILD_DIR)
	$(LOG_MKFAT32)$(MKFAT32_IMAGE) $@ \
	    $(BUILD_DIR)/$(APPS_DIR)/shell.bin $(SDK_HELLO_KLI)

external-kli-image: $(EXTERNAL_KLI_IMG)
	@printf "External KLI FAT32 image: %s\n" "$(EXTERNAL_KLI_IMG)"
	@printf "Run from Shell: run /fat/HELLO.KLI\n"

qemu-external-kli: qemu-check entry-check $(KERNEL_BIN) $(EXTERNAL_KLI_IMG)
	$(LOG_QEMU)qemu-system-aarch64 -machine virt -cpu cortex-a72 -m 128M \
	    -display gtk,gl=off -serial stdio \
	    -global virtio-mmio.force-legacy=false \
	    -kernel $(KERNEL_BIN) \
	    -drive file=$(EXTERNAL_KLI_IMG),if=none,format=raw,id=hd0 \
	    -device virtio-blk-device,drive=hd0 \
	    -device virtio-gpu-device,xres=640,yres=480 \
	    -device qemu-xhci,id=xhci \
	    -device usb-kbd,bus=xhci.0 \
	    -device virtio-mouse-device
