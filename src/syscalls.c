#include "syscalls.h"
#include "idt.h"

// Extern functions
extern void log_info(const char* msg);
extern char keyboard_get_char(void);
extern int elf_exec(const char* path);

static void sys_exit(int code) {
    (void)code;
    log_info("Process exited");
    // For now, just halt - later we'll implement proper process termination
    while(1) __asm__ __volatile__("hlt");
}

// External VGA functions from kernel
extern void vga_put_char(char c);

static int sys_read(char* buffer, int max_len) {
    int i = 0;
    while (i < max_len - 1) {
        char c = keyboard_get_char();
        
        if (c == '\n') {
            buffer[i] = '\0';
            vga_put_char('\n');
            return i;
        }
        
        if (c == '\b') {
            if (i > 0) {
                i--;
                vga_put_char('\b');
            }
            continue;
        }
        
        buffer[i++] = c;
        vga_put_char(c);  // Echo character
    }
    buffer[i] = '\0';
    return i;
}

static void sys_write(char* str) {
    log_info(str);
}

static int sys_exec(const char* path) {
    return elf_exec(path);
}

void syscall_handler(struct registers* regs) {
    // EAX = syscall number
    // EBX, ECX, EDX = arguments
    // Return value in EAX
    
    // Enable interrupts to allow keyboard/timer IRQs during syscalls
    __asm__ __volatile__("sti");
    
    int ret = 0;
    
    switch (regs->eax) {
        case SYS_EXIT:
            sys_exit(regs->ebx);
            break;
        case SYS_READ:
            ret = sys_read((char*)regs->ebx, regs->ecx);
            break;
        case SYS_WRITE:
            sys_write((char*)regs->ebx);
            break;
        case SYS_EXEC:
            ret = sys_exec((const char*)regs->ebx);
            break;
        default:
            log_info("Unknown Syscall");
            ret = -1;
            break;
    }
    
    regs->eax = ret;
}

void syscall_init(void) {
    log_info("Syscalls: Initializing...");
    
    // Register the C handler
    register_interrupt_handler(0x80, syscall_handler);
    
    // Set up the IDT gate with DPL 3 (User Mode)
    // 0xEE = Present, DPL 3, 32-bit Interrupt Gate
    extern void isr128(void);
    idt_set_gate(0x80, (uint32_t)isr128, 0x08, 0xEE);
}
