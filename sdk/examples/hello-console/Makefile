SDK ?= ../..
CROSS_COMPILE ?= aarch64-linux-gnu-
EXTRA_CFLAGS ?=

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

BUILD_DIR ?= build
TARGET_ELF := $(BUILD_DIR)/hello.elf
TARGET_KLI := $(BUILD_DIR)/HELLO.KLI
APP_OBJ := $(BUILD_DIR)/main.o

CFLAGS := -Wall -Wextra -Werror \
          -ffreestanding -nostdlib -nostartfiles \
          -fno-builtin -fno-stack-protector -mgeneral-regs-only \
          -fno-pic -fno-pie \
          -ffunction-sections -fdata-sections \
          -mcpu=cortex-a72 -std=c11 -Os -g \
          -I$(SDK)/include $(EXTRA_CFLAGS)

.PHONY: all clean

all: $(TARGET_KLI)

$(BUILD_DIR):
	mkdir -p $@

$(APP_OBJ): main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_ELF): $(APP_OBJ) \
               $(SDK)/lib/kli1_header.o \
               $(SDK)/lib/crt0.o \
               $(SDK)/lib/libkarm.a \
               $(SDK)/lib/kli1_end.o \
               $(SDK)/linker/image.ld
	bash $(SDK)/tools/check_kli1_relocations.sh \
	    $(APP_OBJ) \
	    $(SDK)/lib/kli1_header.o \
	    $(SDK)/lib/crt0.o \
	    $(SDK)/lib/kli1_end.o
	$(LD) --gc-sections -nostdlib -T $(SDK)/linker/image.ld \
	    $(APP_OBJ) \
	    $(SDK)/lib/kli1_header.o \
	    $(SDK)/lib/crt0.o \
	    $(SDK)/lib/libkarm.a \
	    $(SDK)/lib/kli1_end.o \
	    -o $@

$(TARGET_KLI): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@
	@size=$$(wc -c < $@); \
	if [ $$size -gt 8192 ]; then \
	    printf 'error: %s is %s bytes; KLI1 loader limit is 8192\n' "$@" "$$size" >&2; \
	    exit 1; \
	fi; \
	printf 'KLI1: %s (%s / 8192 bytes)\n' "$@" "$$size"

clean:
	rm -rf $(BUILD_DIR)
