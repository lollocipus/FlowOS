// Simple test program for FlowOS
// This will be loaded and executed by the ELF loader

// Syscall numbers
#define SYS_WRITE 4
#define SYS_EXIT  1

// Make a syscall
static inline int syscall1(int num, int arg1) {
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
    return ret;
}

void _start(void) {
    // This is the entry point
    const char* msg = "Hello from userspace program!\n";
    syscall1(SYS_WRITE, (int)msg);
    
    // Exit
    while(1);  // Infinite loop for now
}
