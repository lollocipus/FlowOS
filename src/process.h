#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"

#define MAX_PROCESSES 256
#define KERNEL_STACK_SIZE 4096
#define USER_STACK_SIZE 4096

typedef enum {
    PROCESS_STATE_UNUSED = 0,
    PROCESS_STATE_CREATED,
    PROCESS_STATE_READY,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED,
    PROCESS_STATE_SLEEPING,
    PROCESS_STATE_TERMINATED
} process_state_t;

// CPU context saved during context switch
struct cpu_context {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;
} __attribute__((packed));

// Process Control Block
struct process {
    uint32_t pid;                    // Process ID
    process_state_t state;           // Current state
    
    struct cpu_context context;      // Saved CPU context
    uint32_t esp;                    // Stack pointer
    uint32_t kernel_stack;           // Kernel stack base
    uint32_t page_directory;         // Page directory physical address
    
    uint32_t sleep_until;            // Timer tick to wake up (for sleeping)
    int32_t exit_code;               // Exit code
    
    struct process* parent;          // Parent process
    struct process* next;            // Next in queue (for scheduler)
    
    char name[32];                   // Process name
};

typedef struct process process_t;

// Process management
void process_init(void);
process_t* process_create(const char* name, void (*entry)(void));
void process_exit(int32_t code);
void process_yield(void);
void process_sleep(uint32_t ms);
process_t* process_get_current(void);
uint32_t process_get_pid(void);

// Scheduler
void scheduler_init(void);
void schedule(void);
void scheduler_add(process_t* proc);
void scheduler_remove(process_t* proc);

#endif
