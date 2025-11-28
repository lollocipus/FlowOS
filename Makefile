# Makefile for FlowOS

# Tools
ASM = nasm
CC = x86_64-linux-gnu-gcc
LD = x86_64-linux-gnu-ld
QEMU = qemu-system-x86_64
GRUB_MKRESCUE = grub-mkrescue

# Flags
ASMFLAGS = -f elf32
CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -c
LDFLAGS = -T linker.ld -m elf_i386

# Files
BUILD_DIR = build
ISO_DIR = $(BUILD_DIR)/isofiles
BOOT_DIR = $(ISO_DIR)/boot
GRUB_DIR = $(BOOT_DIR)/grub
TARGET = $(BOOT_DIR)/flowos.bin
ISO_FILE = $(BUILD_DIR)/FlowOS.iso
GRUB_CFG = grub/grub.cfg
GRUB_CFG_TARGET = $(GRUB_DIR)/grub.cfg

# Source files
ASM_SOURCES = $(wildcard src/*.asm)
C_SOURCES = $(wildcard src/*.c)
OBJS = $(patsubst src/%.asm, $(BUILD_DIR)/%.o, $(ASM_SOURCES)) $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))

.PHONY: all run clean iso

# Ensure required directories exist
DIRS = $(BUILD_DIR) $(ISO_DIR) $(BOOT_DIR) $(GRUB_DIR)
$(DIRS):
	mkdir -p $@

all: $(TARGET)

# Compile assembly files
$(BUILD_DIR)/%.o: src/%.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# Compile C files
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Link the kernel
$(TARGET): $(OBJS) linker.ld | $(BOOT_DIR)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS)

# Copy GRUB configuration into ISO tree
$(GRUB_CFG_TARGET): $(GRUB_CFG) | $(GRUB_DIR)
	cp $< $@

# Create the ISO file
iso: $(TARGET) $(GRUB_CFG_TARGET)
	$(GRUB_MKRESCUE) -o $(ISO_FILE) $(ISO_DIR)

# Run the OS in QEMU
run: iso
	$(QEMU) -cdrom $(ISO_FILE)

# Clean up build files
clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET) $(ISO_FILE) $(ISO_DIR)