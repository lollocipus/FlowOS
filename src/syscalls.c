#include "syscalls.h"
#include "idt.h"

// Extern logging
extern void log_info(const char* msg);

static void sys_write(char* str) {
    log_info(str);
}

void syscall_handler(struct registers* regs) {
    // EAX = syscall number
    // EBX, ECX, EDX = arguments
    
    switch (regs->eax) {
        case SYS_WRITE:
            sys_write((char*)regs->ebx);
            break;
        default:
            log_info("Unknown Syscall");
            break;
    }
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
