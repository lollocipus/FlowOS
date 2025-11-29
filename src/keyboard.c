#include "keyboard.h"
#include "idt.h"
#include "pic.h"

#define KEYBOARD_DATA_PORT   0x60
#define KEYBOARD_STATUS_PORT 0x64

#define KEYBOARD_BUFFER_SIZE 256

static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t buffer_start = 0;
static volatile uint32_t buffer_end = 0;

static const char scancode_to_ascii[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void keyboard_callback(struct registers* regs) {
    (void)regs;

    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Ignore key releases
    if (scancode & 0x80) {
        return;
    }

    char c = scancode_to_ascii[scancode];
    if (c != 0) {
        uint32_t next = (buffer_end + 1) % KEYBOARD_BUFFER_SIZE;
        if (next != buffer_start) {
            keyboard_buffer[buffer_end] = c;
            buffer_end = next;
        }
    }
}

void keyboard_init(void) {
    register_interrupt_handler(33, keyboard_callback);
    
    // Unmask keyboard interrupt
    pic_clear_mask(IRQ_KEYBOARD);
}

bool keyboard_has_input(void) {
    return buffer_start != buffer_end;
}

char keyboard_get_char(void) {
    while (buffer_start == buffer_end) {
        __asm__ __volatile__("hlt");
    }

    char c = keyboard_buffer[buffer_start];
    buffer_start = (buffer_start + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}
