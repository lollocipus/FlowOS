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

# Userspace
USERSPACE_DIR = userspace
USERSPACE_SOURCES = $(wildcard $(USERSPACE_DIR)/*.c)
USERSPACE_BINS = $(patsubst $(USERSPACE_DIR)/%.c, $(BOOT_DIR)/%, $(USERSPACE_SOURCES))
USERSPACE_CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -static
USERSPACE_LDFLAGS = -m elf_i386 -Ttext=0x40000000 -e _start

.PHONY: all run clean iso

all: $(TARGET) $(USERSPACE_BINS)

# Ensure required directories exist
DIRS = $(BUILD_DIR) $(ISO_DIR) $(BOOT_DIR) $(GRUB_DIR)
$(DIRS):
	mkdir -p $@

# Compile assembly files
$(BUILD_DIR)/%.o: src/%.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# Compile C files
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Link the kernel
$(TARGET): $(OBJS) linker.ld | $(BOOT_DIR)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS)

# Compile userspace programs
$(BOOT_DIR)/%: $(USERSPACE_DIR)/%.c | $(BOOT_DIR)
	$(CC) $(USERSPACE_CFLAGS) -c $< -o $(BUILD_DIR)/$*.o
	$(LD) $(USERSPACE_LDFLAGS) -o $@ $(BUILD_DIR)/$*.o

# Copy GRUB configuration into ISO tree
$(GRUB_CFG_TARGET): $(GRUB_CFG) | $(GRUB_DIR)
	cp $< $@

# Create the ISO file
iso: $(TARGET) $(GRUB_CFG_TARGET) $(USERSPACE_BINS)
	$(GRUB_MKRESCUE) -o $(ISO_FILE) $(ISO_DIR)

# Run the OS in QEMU
run: iso
	$(QEMU) -boot d -cdrom $(ISO_FILE) -hda build/disk.img -serial file:build/serial.log

# Clean up build files
clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET) $(ISO_FILE) $(ISO_DIR)