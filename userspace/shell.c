// FlowOS Shell - Simple command line interface

#define SYS_EXIT  1
#define SYS_READ  3
#define SYS_WRITE 4
#define SYS_EXEC  11

// Syscall wrappers
static inline int syscall1(int num, int arg1) {
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
    return ret;
}

static inline int syscall2(int num, int arg1, int arg2) {
    int ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2));
    return ret;
}

static void write(const char* str) {
    syscall1(SYS_WRITE, (int)str);
}

static int read(char* buffer, int max_len) {
    return syscall2(SYS_READ, (int)buffer, max_len);
}

static int exec(const char* path) {
    return syscall1(SYS_EXEC, (int)path);
}

static void exit(int code) {
    syscall1(SYS_EXIT, code);
}

// String helpers
static int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

void _start(void) {
    char buffer[128];
    
    write("\n\n*** FlowOS Shell ***\n");
    write("Type 'help' for available commands\n\n");
    
    while (1) {
        write("FlowOS> ");
        
        int len = read(buffer, sizeof(buffer));
        if (len == 0) continue;
        
        write("\n");
        
        // Parse command
        if (strcmp(buffer, "help") == 0) {
            write("Available commands:\n");
            write("  help  - Show this help\n");
            write("  clear - Clear screen\n");
            write("  test  - Run test program\n");
            write("  exit  - Exit shell\n\n");
        }
        else if (strcmp(buffer, "clear") == 0 || strcmp(buffer, "cls") == 0) {
            // Clear screen by printing newlines
            for (int i = 0; i < 25; i++) {
                write("\n");
            }
        }
        else if (strcmp(buffer, "exit") == 0) {
            write("Goodbye!\n");
            exit(0);
        }
        else if (len > 0) {
            // Try to execute as a program
            char path[128];
            
            // Convert to uppercase for FAT32
            for (int i = 0; buffer[i]; i++) {
                if (buffer[i] >= 'a' && buffer[i] <= 'z') {
                    path[i] = buffer[i] - 32;
                } else {
                    path[i] = buffer[i];
                }
            }
            path[len] = '\0';
            
            int result = exec(path);
            if (result < 0) {
                write("Command not found: ");
                write(buffer);
                write("\n");
            }
        }
    }
}
