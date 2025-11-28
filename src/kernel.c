/*
 * A simple kernel for FlowOS.
 */

// Simple VGA text-mode writer so QEMU window shows progress.
static volatile char* const VGA_MEMORY = (char*)0xB8000;
static const int VGA_COLS = 80;
static const int VGA_ROWS = 25;
static int vga_row = 0;
static int vga_col = 0;
static unsigned char vga_color = 0x0F; // White on black

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

// Function to write a single character to the serial port (COM1)
static void serial_write(char c) {
    // Wait until the transmit buffer is empty (check Transmit Empty bit 5 of Line Status Register)
    // The Line Status Register is at COM1_PORT + 5 (0x3F8 + 5 = 0x3FD)
    while (!(*(volatile char*)0x3FD & 0x20));
    *(volatile char*)0x3F8 = c; // Send the character
}

// Function to print a string to the serial port
static void print_serial(const char* str) {
    for (int i = 0; str[i] != '\0'; ++i) {
        serial_write(str[i]);
    }
}

// The main kernel entry point.
void kmain() {
    // A basic serial port initialization (optional, but good practice)
    // This sets baud rate to 115200 (divisor = 1), 8 data bits, no parity, 1 stop bit
    // See OSDev Wiki for more details: https://wiki.osdev.org/Serial_Ports
    
    // Disable interrupts
    *(char*)(0x3F8 + 1) = 0x00;
    // Enable DLAB (set baud rate divisor)
    *(char*)(0x3F8 + 3) = 0x80;
    // Set divisor to 1 (115200 baud)
    *(char*)(0x3F8 + 0) = 0x01;
    *(char*)(0x3F8 + 1) = 0x00;
    // Disable DLAB, set 8 data bits, no parity, 1 stop bit
    *(char*)(0x3F8 + 3) = 0x03;
    // Enable FIFO, clear them, with 14-byte threshold
    *(char*)(0x3F8 + 2) = 0xC7;
    // IRQs enabled, RTS/DSR set
    *(char*)(0x3F8 + 4) = 0x0B;


    const char* msg = "FlowOS is booting...\n";
    print_serial(msg); // Print to serial for debugging
    print_vga(msg);    // Print to VGA so it is visible in QEMU window

    // Infinite loop to keep the kernel running
    while(1) {}
}