# The directory that contains FreeRTOS source code
FREERTOS_ROOT := ../FreeRTOS_copy

# Demo code
DEMO_PROJECT := .

# FreeRTOS kernel
KERNEL_DIR := $(FREERTOS_ROOT)/Source
KERNEL_PORT_DIR := $(KERNEL_DIR)/portable/GCC/ARM_CM3

# Where to store all the generated files (objects, elf and map)
OUTPUT_DIR := ./Output

DEMO_NAME := timeline_demo
ELF := $(OUTPUT_DIR)/$(DEMO_NAME).elf
MAP := $(OUTPUT_DIR)/$(DEMO_NAME).map

# Compiler toolchain
CC := arm-none-eabi-gcc
LD := arm-none-eabi-gcc
SIZE := arm-none-eabi-size

# Target embedded board and CPU
MACHINE := mps2-an385
CPU := cortex-m3

# Selected timeline profile and debug verbosity.
# Example: make all PROFILE=3 TL_DEBUG=1
PROFILE ?= 4
TL_DEBUG ?= 1
TEST_TIMEOUT ?= 3.0

# Use -s to connect to gdb port 1234 and -S to wait before executing
QEMU_FLAGS_DBG = -s -S

INCLUDE_DIRS = -I$(KERNEL_DIR)/include -I$(KERNEL_PORT_DIR)
INCLUDE_DIRS += -I$(DEMO_PROJECT)/include
INCLUDE_DIRS += -I$(DEMO_PROJECT)/board

VPATH += $(KERNEL_DIR) $(KERNEL_PORT_DIR) $(KERNEL_DIR)/portable/MemMang
VPATH += $(DEMO_PROJECT) $(DEMO_PROJECT)/config $(DEMO_PROJECT)/board

# Include paths. See INCLUDE_DIRS
CFLAGS = $(INCLUDE_DIRS)

# Build-time switches for timeline profile and debug prints.
CFLAGS += -DCONFIG_PROFILE=$(PROFILE)
CFLAGS += -DDEBUG=$(TL_DEBUG)

# Don't include standard libraries
CFLAGS += -ffreestanding

# Target ARM cortex M3 processor
CFLAGS += -mcpu=$(CPU)
# Use the thumb 16 bits instruction set. Required for cortex-M
CFLAGS += -mthumb

# Print all the most common warnings
CFLAGS += -Wall

# Print extra warnings, not included in -Wall
CFLAGS += -Wextra

# Print warnings about variable shadowing (var name = outer scope var name)
CFLAGS += -Wshadow

# Include debug information (-g) with maximum level of detail (3)
CFLAGS += -g3

# Optimize (-O) the size (s) of the generated executable
CFLAGS += -Os

# Place each function in a dedicated section (see -gc-sections in LDFLAGS)
CFLAGS += -ffunction-sections

# Place each variable in a dedicated section (see -gc-sections in LDFLAGS)
CFLAGS += -fdata-sections

# Specify the linker script
LDFLAGS = -T ./mps2_an385.ld

# Don't use the standard start-up files
LDFLAGS += -nostartfiles

# Optimize compilation to reduce memory
LDFLAGS += -specs=nano.specs

# Optimize compilation targeting a system without any operating system
LDFLAGS += -specs=nosys.specs

# Generate map file with memory layout. Pass option to the linker (-Xlinker)
LDFLAGS += -Xlinker -Map=$(MAP)

# Remove unused sections. Used with -ffunction-sections and -fdata-sections
# optimized memory. Pass option to the linker (-Xlinker)
LDFLAGS += -Xlinker --gc-sections

# Link specifically for cortex M3 and thumb instruction set
LDFLAGS += -mcpu=$(CPU) -mthumb

# Kernel files
SOURCE_FILES += $(KERNEL_DIR)/list.c
SOURCE_FILES += $(KERNEL_DIR)/tasks.c
SOURCE_FILES += $(KERNEL_DIR)/queue.c
SOURCE_FILES += $(KERNEL_DIR)/timers.c
SOURCE_FILES += $(KERNEL_DIR)/event_groups.c
SOURCE_FILES += $(KERNEL_DIR)/stream_buffer.c
SOURCE_FILES += $(KERNEL_DIR)/portable/MemMang/heap_4.c
SOURCE_FILES += $(KERNEL_PORT_DIR)/port.c

# Demo files
SOURCE_FILES += $(DEMO_PROJECT)/app_main.c
SOURCE_FILES += $(DEMO_PROJECT)/config/timeline_config.c
SOURCE_FILES += $(DEMO_PROJECT)/config/timeline_scheduler.c
SOURCE_FILES += $(DEMO_PROJECT)/config/timeline_kernel_hooks.c
SOURCE_FILES += $(DEMO_PROJECT)/board/uart.c

# Start-up code
SOURCE_FILES += $(DEMO_PROJECT)/startup_mps2_an385.c

# Create list of object files with the same names of the sources
OBJS = $(SOURCE_FILES:%.c=%.o)

# Remove path from object filename
OBJS_NOPATH = $(notdir $(OBJS))

# Prepend output dir to object filenames
OBJS_OUTPUT = $(OBJS_NOPATH:%.o=$(OUTPUT_DIR)/%.o)

all: $(ELF)

$(ELF): $(OBJS_OUTPUT) ./mps2_an385.ld Makefile
	echo "\n\n--- Final linking ---\n"
	$(LD) $(LDFLAGS) $(OBJS_OUTPUT) -o $(ELF)
	$(SIZE) $(ELF)

$(OUTPUT_DIR)/%.o : %.c Makefile $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

cleanobjs:
	rm -f $(OUTPUT_DIR)/*.o

clean:
	rm -rf $(ELF) $(MAP) $(OUTPUT_DIR)/*.o $(OUTPUT_DIR)

run:
	qemu-system-arm -machine $(MACHINE) -cpu $(CPU) -kernel \
	$(ELF) -monitor none -nographic -serial stdio

run-single:
	@if [ -z "$(TEST)" ]; then \
		echo "Usage: make run-single TEST=<1..11> [TL_DEBUG=1]"; \
		exit 1; \
	fi
	$(MAKE) cleanobjs all PROFILE=$(TEST) TL_DEBUG=$(TL_DEBUG)
	qemu-system-arm -machine $(MACHINE) -cpu $(CPU) -kernel \
	$(ELF) -monitor none -nographic -serial stdio

test-one:
	@if [ -z "$(TEST)" ]; then \
		echo "Usage: make test-one TEST=<1..11> [TEST_TIMEOUT=3.0]"; \
		exit 1; \
	fi
	python3 ./tools/timeline_test_suite.py --test $(TEST) --timeout $(TEST_TIMEOUT)

test-all:
	python3 ./tools/timeline_test_suite.py --all --timeout $(TEST_TIMEOUT)

debug: all
	qemu-system-arm -machine $(MACHINE) -cpu $(CPU) -kernel \
	$(ELF) -monitor none -nographic -serial stdio $(QEMU_FLAGS_DBG)

gdb_start:
	gdb-multiarch $(ELF)
