#include "process.h"
#include "heap.h"
#include "timer.h"
#include "idt.h"
#include "paging.h"

// Process table
static process_t process_table[MAX_PROCESSES];
static process_t* current_process = NULL;
static process_t* ready_queue_head = NULL;
static process_t* ready_queue_tail = NULL;
static uint32_t next_pid = 1;

// External context switch function (defined in switch.asm)
extern void context_switch(uint32_t* old_esp, uint32_t new_esp);

// Idle process - runs when no other process is ready
static void idle_process(void) {
    while (1) {
        hlt();
    }
}

// Process wrapper to handle exit
static void process_wrapper(void (*entry)(void)) {
    entry();
    process_exit(0);
}

void process_init(void) {
    // Clear process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = PROCESS_STATE_UNUSED;
        process_table[i].pid = 0;
    }

    // Create idle process (PID 0)
    process_t* idle = &process_table[0];
    idle->pid = 0;
    idle->state = PROCESS_STATE_READY;
    idle->kernel_stack = (uint32_t)kmalloc(KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;
    idle->page_directory = paging_kernel_pd_phys();
    idle->parent = NULL;
    idle->next = NULL;
    
    // Set up idle process stack
    uint32_t* stack = (uint32_t*)idle->kernel_stack;
    stack[-1] = (uint32_t)idle_process;  // Return address (entry point)
    stack[-2] = 0;  // EBP
    stack[-3] = 0;  // EBX
    stack[-4] = 0;  // ESI
    stack[-5] = 0;  // EDI
    idle->esp = (uint32_t)&stack[-5];

    // Copy name
    const char* name = "idle";
    for (int i = 0; i < 31 && name[i]; i++) {
        idle->name[i] = name[i];
        idle->name[i + 1] = '\0';
    }

    current_process = idle;
}

static process_t* find_free_slot(void) {
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_STATE_UNUSED) {
            return &process_table[i];
        }
    }
    return NULL;
}

process_t* process_create(const char* name, void (*entry)(void)) {
    process_t* proc = find_free_slot();
    if (!proc) return NULL;

    proc->pid = next_pid++;
    proc->state = PROCESS_STATE_CREATED;
    proc->parent = current_process;
    proc->next = NULL;
    proc->exit_code = 0;
    proc->sleep_until = 0;
    proc->page_directory = paging_clone_pd();
    if (!proc->page_directory) {
        kfree((void*)(proc->kernel_stack - KERNEL_STACK_SIZE));
        proc->state = PROCESS_STATE_UNUSED;
        return NULL;
    }

    // Allocate kernel stack
    proc->kernel_stack = (uint32_t)kmalloc(KERNEL_STACK_SIZE);
    if (!proc->kernel_stack) {
        proc->state = PROCESS_STATE_UNUSED;
        return NULL;
    }
    proc->kernel_stack += KERNEL_STACK_SIZE;  // Stack grows down

    // Set up initial stack frame
    uint32_t* stack = (uint32_t*)proc->kernel_stack;
    
    // Set up stack for context_switch to work
    // When context_switch returns, it will pop these and "return" to process_wrapper
    stack[-1] = (uint32_t)entry;           // Argument to process_wrapper
    stack[-2] = 0;                          // Fake return address
    stack[-3] = (uint32_t)process_wrapper;  // EIP - entry point
    stack[-4] = 0;                          // EBP
    stack[-5] = 0;                          // EBX
    stack[-6] = 0;                          // ESI
    stack[-7] = 0;                          // EDI
    
    proc->esp = (uint32_t)&stack[-7];

    // Copy name
    int i;
    for (i = 0; i < 31 && name[i]; i++) {
        proc->name[i] = name[i];
    }
    proc->name[i] = '\0';

    // Add to ready queue
    proc->state = PROCESS_STATE_READY;
    scheduler_add(proc);

    return proc;
}

void process_exit(int32_t code) {
    if (!current_process || current_process->pid == 0) {
        // Can't exit idle process
        return;
    }

    current_process->exit_code = code;
    current_process->state = PROCESS_STATE_TERMINATED;

    // Free kernel stack
    kfree((void*)(current_process->kernel_stack - KERNEL_STACK_SIZE));

    // Switch to another process
    schedule();
}

void process_yield(void) {
    if (current_process && current_process->state == PROCESS_STATE_RUNNING) {
        current_process->state = PROCESS_STATE_READY;
        scheduler_add(current_process);
    }
    schedule();
}

void process_sleep(uint32_t ms) {
    if (!current_process) return;

    uint32_t ticks = (ms * 100) / 1000;  // Convert ms to ticks (100 Hz timer)
    if (ticks == 0) ticks = 1;

    current_process->sleep_until = timer_get_ticks() + ticks;
    current_process->state = PROCESS_STATE_SLEEPING;
    
    schedule();
}

process_t* process_get_current(void) {
    return current_process;
}

uint32_t process_get_pid(void) {
    return current_process ? current_process->pid : 0;
}

// Scheduler implementation
void scheduler_init(void) {
    ready_queue_head = NULL;
    ready_queue_tail = NULL;
}

void scheduler_add(process_t* proc) {
    if (!proc) return;
    
    proc->next = NULL;
    
    if (!ready_queue_tail) {
        ready_queue_head = proc;
        ready_queue_tail = proc;
    } else {
        ready_queue_tail->next = proc;
        ready_queue_tail = proc;
    }
}

void scheduler_remove(process_t* proc) {
    if (!proc || !ready_queue_head) return;

    if (ready_queue_head == proc) {
        ready_queue_head = proc->next;
        if (!ready_queue_head) {
            ready_queue_tail = NULL;
        }
        proc->next = NULL;
        return;
    }

    process_t* prev = ready_queue_head;
    while (prev->next && prev->next != proc) {
        prev = prev->next;
    }

    if (prev->next == proc) {
        prev->next = proc->next;
        if (ready_queue_tail == proc) {
            ready_queue_tail = prev;
        }
        proc->next = NULL;
    }
}

static void wake_sleeping_processes(void) {
    uint32_t current_tick = timer_get_ticks();
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_STATE_SLEEPING) {
            if (current_tick >= process_table[i].sleep_until) {
                process_table[i].state = PROCESS_STATE_READY;
                scheduler_add(&process_table[i]);
            }
        }
    }
}

void schedule(void) {
    if (!current_process) return;

    // Wake up sleeping processes
    wake_sleeping_processes();

    // Get next process from ready queue
    process_t* next = ready_queue_head;
    
    if (next) {
        ready_queue_head = next->next;
        if (!ready_queue_head) {
            ready_queue_tail = NULL;
        }
        next->next = NULL;
    } else {
        // No ready process, run idle
        next = &process_table[0];
    }

    if (next == current_process) {
        current_process->state = PROCESS_STATE_RUNNING;
        return;
    }

    // Perform context switch
    process_t* prev = current_process;
    current_process = next;
    next->state = PROCESS_STATE_RUNNING;

    if (prev->page_directory != current_process->page_directory) {
        paging_switch(current_process->page_directory);
    }

    context_switch(&prev->esp, next->esp);
}
