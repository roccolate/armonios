# ArmoniOS top-level build policy and post-Makefile composition.
#
# GNU make prefers this file over Makefile. The main build remains in Makefile;
# cross-cutting foundation objects are appended here so every board links the
# same storage and ABI contracts without duplicating board conditionals.

KERNEL_SIZE_LIMIT ?= 131072

include Makefile

FOUNDATION_OBJS := \
    $(BUILD_DIR)/drivers/storage/block_device.o \
    $(BUILD_DIR)/kernel/fat32_device.o \
    $(BUILD_DIR)/kernel/fat32_directory.o \
    $(BUILD_DIR)/kernel/vfs_metadata.o \
    $(BUILD_DIR)/kernel/syscall_vfs_metadata.o

ifeq ($(filter $(BUILD_DIR)/drivers/storage/mbr.o,$(OBJS)),)
FOUNDATION_OBJS += $(BUILD_DIR)/drivers/storage/mbr.o
endif

OBJS += $(FOUNDATION_OBJS)
DEPS += $(FOUNDATION_OBJS:.o=.d)

-include $(FOUNDATION_OBJS:.o=.d)

$(KERNEL_ELF): $(FOUNDATION_OBJS)
