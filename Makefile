OUTPUT_DIR := build
IMAGE := $(OUTPUT_DIR)/timeline_demo.elf

CC := arm-none-eabi-gcc
SIZE := arm-none-eabi-size

QEMU := qemu-system-arm
QEMU_MACHINE := mps2-an385
QEMU_CPU := cortex-m3

CFLAGS := -mthumb -mcpu=$(QEMU_CPU) -ffreestanding
CFLAGS += -g3 -Os -ffunction-sections -fdata-sections
CFLAGS += -Wall -Wextra -Wshadow -Wno-unused-parameter -Wno-unused-value
CFLAGS += -MMD -MP
CFLAGS += -I.
CFLAGS += -IFreeRTOS_copy/Source/include
CFLAGS += -IFreeRTOS_copy/Source/portable/GCC/ARM_CM3
CFLAGS += -IFreeRTOS_copy/Demo/CORTEX_MPS2_QEMU_IAR_GCC
CFLAGS += -IFreeRTOS_copy/Demo/CORTEX_MPS2_QEMU_IAR_GCC/CMSIS

LDFLAGS := -T FreeRTOS_copy/Demo/CORTEX_MPS2_QEMU_IAR_GCC/build/gcc/mps2_m3.ld
LDFLAGS += -Wl,-Map,$(OUTPUT_DIR)/timeline_demo.map
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -nostartfiles -specs=nano.specs -specs=nosys.specs

SRCS := \
	app_main.c \
	timeline_scheduler.c \
	timeline_kernel_hooks.c \
	timeline_config.c \
	FreeRTOS_copy/Source/tasks.c \
	FreeRTOS_copy/Source/list.c \
	FreeRTOS_copy/Source/queue.c \
	FreeRTOS_copy/Source/timers.c \
	FreeRTOS_copy/Source/event_groups.c \
	FreeRTOS_copy/Source/stream_buffer.c \
	FreeRTOS_copy/Source/portable/MemMang/heap_4.c \
	FreeRTOS_copy/Source/portable/GCC/ARM_CM3/port.c \
	FreeRTOS_copy/Demo/CORTEX_MPS2_QEMU_IAR_GCC/build/gcc/startup_gcc.c \
	FreeRTOS_copy/Demo/CORTEX_MPS2_QEMU_IAR_GCC/build/gcc/printf-stdarg.c

OBJS := $(patsubst %.c,$(OUTPUT_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

all: $(IMAGE)

$(IMAGE): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
	$(SIZE) $@

$(OUTPUT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(IMAGE)
	$(QEMU) -machine $(QEMU_MACHINE) -cpu $(QEMU_CPU) \
	-kernel $(IMAGE) -monitor none -nographic -serial stdio

clean:
	rm -rf $(OUTPUT_DIR)

-include $(DEPS)

.PHONY: all run clean
