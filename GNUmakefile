# ArmoniOS top-level build policy and post-Makefile composition.
#
# GNU make prefers this file over Makefile. The main build remains in Makefile;
# cross-cutting foundation objects are appended here so every board links the
# same storage contract without duplicating board conditionals.

# 128 KiB gives the kernel, embedded applications, and desktop foundations
# enough development headroom while retaining a hard, measurable size gate.
KERNEL_SIZE_LIMIT ?= 131072

include Makefile

STORAGE_FOUNDATION_OBJS := \
    $(BUILD_DIR)/drivers/storage/block_device.o \
    $(BUILD_DIR)/kernel/fat32_device.o \
    $(BUILD_DIR)/kernel/fat32_directory.o

# RPi4 diagnostic builds already add mbr.o through STORAGE_DEV. Every other
# production build now needs it because kernel storage can open bounded MBR
# partition views.
ifeq ($(filter $(BUILD_DIR)/drivers/storage/mbr.o,$(OBJS)),)
STORAGE_FOUNDATION_OBJS += $(BUILD_DIR)/drivers/storage/mbr.o
endif

OBJS += $(STORAGE_FOUNDATION_OBJS)
DEPS += $(STORAGE_FOUNDATION_OBJS:.o=.d)

# The original dependency include was parsed inside Makefile before the objects
# above were appended.
-include $(STORAGE_FOUNDATION_OBJS:.o=.d)

# The original kernel target was parsed before the appended object list. Add the
# prerequisites explicitly; its deferred link recipe sees the updated OBJS.
$(KERNEL_ELF): $(STORAGE_FOUNDATION_OBJS)
