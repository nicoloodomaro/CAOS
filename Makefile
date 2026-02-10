# --- Configurazione Toolchain ---
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

# --- Percorsi (Basati sulla tua struttura) ---
# Project root
SRC_DIR = source
INC_DIR = include

# FreeRTOS Paths (Adatta se la cartella interna ha un nome diverso)
FREERTOS_ROOT = FreeRTOS/FreeRTOS/Source
PORT_DIR = $(FREERTOS_ROOT)/portable/GCC/ARM_CM3

# --- File Sorgenti ---
# 1. I tuoi file
# 1. I tuoi file
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/timeline_config.c \
       $(SRC_DIR)/timeline_scheduler.c \
       $(SRC_DIR)/startup.c \
       $(SRC_DIR)/utils.c

# 2. Kernel FreeRTOS Core
SRCS += $(FREERTOS_ROOT)/tasks.c \
        $(FREERTOS_ROOT)/list.c \
        $(FREERTOS_ROOT)/queue.c \
        $(FREERTOS_ROOT)/timers.c

# 3. Kernel Port (Specifico per MPS2/Cortex-M3) e Heap
SRCS += $(PORT_DIR)/port.c \
        $(FREERTOS_ROOT)/portable/MemMang/heap_4.c

# --- Include Directories ---
INCLUDES = -I$(INC_DIR) \
           -I$(FREERTOS_ROOT)/include \
           -I$(PORT_DIR)

# --- Flag di Compilazione ---
# MPS2 AN385 simula un Cortex-M3
CFLAGS = -mcpu=cortex-m3 -mthumb -g -O0 -Wall
CFLAGS += $(INCLUDES)

# --- Flag del Linker ---
LDFLAGS = -mcpu=cortex-m3 -mthumb -T$(SRC_DIR)/mps2_m3.ld \
          -specs=nano.specs -specs=nosys.specs \
          -Wl,--gc-sections

# --- Output ---
TARGET = build/scheduler_demo
OBJS = $(SRCS:.c=.o)

# --- Regole ---
all: $(TARGET).elf

$(TARGET).elf: $(OBJS)
	@mkdir -p build
	$(CC) $(LDFLAGS) -o $@ $^
	$(OBJCOPY) -O binary $@ $(TARGET).bin
	@echo "Build Completata: $@"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(SRC_DIR)/*.o

run: $(TARGET).elf
	@echo "--- Avvio QEMU MPS2 (Ctrl+A, poi X per uscire) ---"
	qemu-system-arm -machine mps2-an385 -cpu cortex-m3 -nographic -kernel $(TARGET).elf

.PHONY: all clean