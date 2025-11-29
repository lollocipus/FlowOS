/*
 * FlowOS Kernel
 */

#include "types.h"
#include "gdt.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "process.h"
#include "pic.h"
#include "vfs.h"
#include "ata.h"
#include "fat32.h"
#include "syscalls.h"
#include "elf.h"

// VGA text-mode driver
static volatile char* const VGA_MEMORY = (char*)0xB8000;
static const int VGA_COLS = 80;
static const int VGA_ROWS = 25;
static int vga_row = 0;
static int vga_col = 0;
static uint8_t vga_color = 0x0F; // White on black

static int kstrlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

static void vga_clear(void) {
    for (int row = 0; row < VGA_ROWS; ++row) {
        for (int col = 0; col < VGA_COLS; ++col) {
            const int idx = (row * VGA_COLS + col) * 2;
            VGA_MEMORY[idx] = ' ';
            VGA_MEMORY[idx + 1] = vga_color;
        }
    }
    vga_row = 0;
    vga_col = 0;
}

static void vga_set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (row >= VGA_ROWS) row = VGA_ROWS - 1;
    if (col < 0) col = 0;
    if (col >= VGA_COLS) col = VGA_COLS - 1;
    vga_row = row;
    vga_col = col;
}

static void vga_put_char(char c) {
    if (c == '\n') {
        vga_col = 0;
        if (++vga_row >= VGA_ROWS) {
            vga_row = 0;
        }
        return;
    }

    const int idx = (vga_row * VGA_COLS + vga_col) * 2;
    VGA_MEMORY[idx] = c;
    VGA_MEMORY[idx + 1] = vga_color;

    if (++vga_col >= VGA_COLS) {
        vga_col = 0;
        if (++vga_row >= VGA_ROWS) {
            vga_row = 0;
        }
    }
}

static void print_vga(const char* str) {
    for (int i = 0; str[i] != '\0'; ++i) {
        vga_put_char(str[i]);
    }
}

static void vga_write_at(int row, int col, const char* str) {
    vga_set_cursor(row, col);
    print_vga(str);
}

static void vga_write_centered(int row, const char* str) {
    const int len = kstrlen(str);
    int col = (VGA_COLS - len) / 2;
    if (col < 0) {
        col = 0;
    }
    vga_write_at(row, col, str);
}

static void uitoa(uint32_t value, char* buffer) {
    char tmp[10];
    int idx = 0;
    if (value == 0) {
        buffer[idx++] = '0';
    } else {
        while (value > 0) {
            tmp[idx++] = '0' + (value % 10);
            value /= 10;
        }
        for (int i = 0; i < idx; ++i) {
            buffer[i] = tmp[idx - i - 1];
        }
    }
    buffer[idx] = '\0';
}

static void draw_loading_bar(int percent) {
    const int row = 13;
    const int width = 40;
    const int col = (VGA_COLS - width) / 2;
    const int inner_width = width - 2;
    const int filled = (percent * inner_width) / 100;

    vga_write_at(row, col, "[");
    vga_write_at(row, col + width - 1, "]");
    for (int i = 0; i < inner_width; ++i) {
        const int idx = ((row * VGA_COLS) + col + 1 + i) * 2;
        const char fill_char = (i < filled) ? '#' : ' ';
        VGA_MEMORY[idx] = fill_char;
        VGA_MEMORY[idx + 1] = vga_color;
    }

    char percent_text[8];
    uitoa(percent, percent_text);
    char label[16];
    label[0] = '\0';

    int l_idx = 0;
    for (; percent_text[l_idx] != '\0'; ++l_idx) {
        label[l_idx] = percent_text[l_idx];
    }
    label[l_idx++] = '%';
    label[l_idx] = '\0';

    vga_write_centered(row + 1, label);
}

static void draw_box(int top, int left, int height, int width) {
    for (int col = left; col < left + width; ++col) {
        int idx = (top * VGA_COLS + col) * 2;
        VGA_MEMORY[idx] = '-';
        VGA_MEMORY[idx + 1] = vga_color;
        idx = ((top + height - 1) * VGA_COLS + col) * 2;
        VGA_MEMORY[idx] = '-';
        VGA_MEMORY[idx + 1] = vga_color;
    }
    for (int row = top; row < top + height; ++row) {
        int idx = (row * VGA_COLS + left) * 2;
        VGA_MEMORY[idx] = '|';
        VGA_MEMORY[idx + 1] = vga_color;
        idx = (row * VGA_COLS + left + width - 1) * 2;
        VGA_MEMORY[idx] = '|';
        VGA_MEMORY[idx + 1] = vga_color;
    }
    int idx = (top * VGA_COLS + left) * 2;
    VGA_MEMORY[idx] = '+';
    idx = (top * VGA_COLS + left + width - 1) * 2;
    VGA_MEMORY[idx] = '+';
    idx = ((top + height - 1) * VGA_COLS + left) * 2;
    VGA_MEMORY[idx] = '+';
    idx = ((top + height - 1) * VGA_COLS + left + width - 1) * 2;
    VGA_MEMORY[idx] = '+';
}

static void draw_input_field(int row, int col, int width) {
    for (int i = 0; i < width; ++i) {
        int idx = (row * VGA_COLS + col + i) * 2;
        VGA_MEMORY[idx] = '_';
        VGA_MEMORY[idx + 1] = vga_color;
    }
}

static int read_input(int row, int col, char* buffer, int max_len, int is_password) {
    int pos = 0;
    char c;

    draw_input_field(row, col, max_len);

    while (1) {
        c = keyboard_get_char();

        if (c == '\n') {
            buffer[pos] = '\0';
            return pos;
        }

        if (c == '\b') {
            if (pos > 0) {
                pos--;
                int idx = (row * VGA_COLS + col + pos) * 2;
                VGA_MEMORY[idx] = '_';
                VGA_MEMORY[idx + 1] = vga_color;
            }
            continue;
        }

        if (pos < max_len - 1 && c >= 32 && c < 127) {
            buffer[pos] = c;
            int idx = (row * VGA_COLS + col + pos) * 2;
            VGA_MEMORY[idx] = is_password ? '*' : c;
            VGA_MEMORY[idx + 1] = vga_color;
            pos++;
        }
    }
}

static char login_username[32];
static char login_password[32];

static void show_login_screen(void) {
    vga_clear();

    int box_width = 40;
    int box_height = 12;
    int box_left = (VGA_COLS - box_width) / 2;
    int box_top = 6;

    draw_box(box_top, box_left, box_height, box_width);

    vga_write_centered(box_top + 2, "FlowOS Login");

    int label_col = box_left + 4;
    int field_col = box_left + 15;
    int field_width = 20;

    vga_write_at(box_top + 4, label_col, "Username:");
    draw_input_field(box_top + 4, field_col, field_width);

    vga_write_at(box_top + 6, label_col, "Password:");
    draw_input_field(box_top + 6, field_col, field_width);

    vga_write_centered(box_top + 9, "Press ENTER after each field");

    while (1) {
        read_input(box_top + 4, field_col, login_username, field_width, 0);
        read_input(box_top + 6, field_col, login_password, field_width, 1);

        if (kstrlen(login_username) > 0 && kstrlen(login_password) > 0) {
            break;
        }
        
        vga_write_centered(box_top + 9, "Invalid credentials!");
        timer_wait(100);
        vga_write_centered(box_top + 9, "                             ");
        vga_write_centered(box_top + 9, "Press ENTER after each field");
        
        // Clear inputs
        draw_input_field(box_top + 4, field_col, field_width);
        draw_input_field(box_top + 6, field_col, field_width);
    }

    vga_write_centered(box_top + 9, "                             ");
    vga_write_centered(box_top + 9, "Authenticating...");
    vga_write_centered(box_top + 9, "Authenticating...");
    timer_wait(100);
}

static void show_booted_message(void) {
    vga_clear();

    int box_width = 50;
    int box_height = 7;
    int box_left = (VGA_COLS - box_width) / 2;
    int box_top = 9;

    draw_box(box_top, box_left, box_height, box_width);

    uint8_t old_color = vga_color;
    vga_color = 0x0A; // Green on black
    vga_write_centered(box_top + 3, "FlowOS Booted");
    vga_color = old_color;

    char welcome[48] = "Welcome, ";
    int i = 9;
    for (int j = 0; login_username[j] != '\0' && i < 46; ++j) {
        welcome[i++] = login_username[j];
    }
    welcome[i++] = '!';
    welcome[i] = '\0';

    vga_write_centered(box_top + box_height + 2, welcome);
}

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);  // Disable interrupts
    outb(0x3F8 + 3, 0x80);  // Enable DLAB
    outb(0x3F8 + 0, 0x01);  // Divisor low byte (115200 baud)
    outb(0x3F8 + 1, 0x00);  // Divisor high byte
    outb(0x3F8 + 3, 0x03);  // 8 bits, no parity, 1 stop bit
    outb(0x3F8 + 2, 0xC7);  // Enable FIFO
    outb(0x3F8 + 4, 0x0B);  // IRQs enabled, RTS/DSR set
}

static void serial_write(char c) {
    while (!(inb(0x3FD) & 0x20));
    outb(0x3F8, c);
}

static void print_serial(const char* str) {
    for (int i = 0; str[i] != '\0'; ++i) {
        serial_write(str[i]);
    }
}

void log_info(const char* msg) {
    print_serial("[INFO] ");
    print_serial(msg);
    print_serial("\n");
    
    // Also print to bottom of VGA
    int row = VGA_ROWS - 1;
    // Clear line
    for (int col = 0; col < VGA_COLS; col++) {
        int idx = (row * VGA_COLS + col) * 2;
        VGA_MEMORY[idx] = ' ';
        VGA_MEMORY[idx + 1] = 0x07; // Light grey on black
    }
    vga_write_at(row, 0, "[INFO] ");
    vga_write_at(row, 7, msg);
}

// External symbol for heap placement
extern uint32_t _end;

// Kernel interrupt stack for TSS
static uint8_t kernel_interrupt_stack[8192] __attribute__((aligned(16)));

void kmain(struct multiboot_info* mboot) {
    // Initialize serial for debug output
    serial_init();
    log_info("FlowOS: Starting kernel...");

    // Initialize VGA and show splash
    vga_clear();
    vga_write_centered(8, "FlowOS is initializing...");
    vga_write_centered(10, "Please wait...");

    // Initialize GDT
    vga_write_at(24, 0, "Initializing GDT...");
    log_info("FlowOS: Initializing GDT...");
    gdt_init();
    
    // Set up TSS with kernel stack for interrupts from Ring 3
    extern void write_tss(int32_t num, uint16_t ss0, uint32_t esp0);
    uint32_t kernel_stack_top = (uint32_t)kernel_interrupt_stack + sizeof(kernel_interrupt_stack);
    write_tss(5, 0x10, kernel_stack_top);
    log_info("FlowOS: TSS configured with kernel interrupt stack");

    // Initialize IDT
    vga_write_at(24, 0, "Initializing IDT...");
    log_info("FlowOS: Initializing IDT...");
    idt_init();

    log_info("FlowOS: Remapping PIC...");
    pic_remap(0x20, 0x28);

    // Initialize Physical Memory Manager
    vga_write_at(24, 0, "Initializing PMM...");
    log_info("FlowOS: Initializing PMM...");
    pmm_init(mboot);



    // Initialize Paging
    vga_write_at(24, 0, "Initializing Paging...");
    log_info("FlowOS: Initializing paging...");
    paging_init();

    log_info("FlowOS: Allocating kernel heap...");
    const uint32_t KERNEL_HEAP_VIRT = 0xC0000000;
    const uint32_t HEAP_PAGES = 256;
    uint32_t heap_phys = pmm_alloc_pages(HEAP_PAGES);
    if (!heap_phys) {
        vga_write_centered(12, "CRITICAL ERROR: Heap OOM");
        log_info("Heap OOM");
        hlt();
    }
    for (uint32_t i = 0; i < HEAP_PAGES; i++) {
        paging_map_page(KERNEL_HEAP_VIRT + i * PAGE_SIZE, heap_phys + i * PAGE_SIZE, PAGE_PRESENT | PAGE_WRITE);
    }

    log_info("FlowOS: Initializing heap...");
    heap_init(KERNEL_HEAP_VIRT, HEAP_PAGES * PAGE_SIZE);

    // Initialize ATA
    log_info("FlowOS: Initializing ATA...");
    ata_init();

    // Initialize FAT32
    log_info("FlowOS: Initializing FAT32...");
    fat32_init();
    
    // Debug: List files in root directory
    log_info("FAT32: Files in root directory:");
    for (int i = 0; i < 20; i++) {
        struct dirent* entry = vfs_readdir(fs_root, i);
        if (!entry) break;
        log_info(entry->name);
        kfree(entry);
    }

    // Initialize timer (100 Hz)
    vga_write_at(24, 0, "Initializing Timer...");
    log_info("FlowOS: Initializing timer...");
    timer_init(100);

    // Initialize keyboard
    log_info("FlowOS: Initializing keyboard...");
    keyboard_init();

    // Initialize process management
    vga_write_at(24, 0, "Initializing Process Manager...");
    log_info("FlowOS: Initializing process management...");
    process_init();
    scheduler_init();
    
    // Initialize Syscalls
    syscall_init();

    // Enable interrupts
    log_info("FlowOS: Enabling interrupts...");
    sti();

    log_info("FlowOS: Kernel initialized successfully!");

    // Show boot screen
    vga_clear();
    vga_write_centered(8, "FlowOS is initializing...");
    vga_write_centered(11, "Loading core modules...");

    for (int percent = 0; percent <= 100; percent += 10) {
        draw_loading_bar(percent);
        timer_wait(15);
    }

    // Login screen
    show_login_screen();

    // Boot complete
    show_booted_message();

    log_info("FlowOS: Boot complete! Loading test program...");
    
    // Try to load and execute a test ELF program
    if (elf_exec("TEST") < 0) {
        log_info("ELF: Failed to load test program");
    }
    
    log_info("FlowOS: System ready.");

    // Main loop
    while (1) {
        hlt();
    }
}
