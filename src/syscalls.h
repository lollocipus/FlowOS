#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "types.h"
#include "idt.h"

#define SYS_EXIT  1
#define SYS_READ  3
#define SYS_WRITE 4
#define SYS_EXEC  11

void syscall_init(void);
void syscall_handler(struct registers* regs);

#endif
