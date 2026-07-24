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
