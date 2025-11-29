#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "types.h"
#include "idt.h"

#define SYS_WRITE 4

void syscall_init(void);
void syscall_handler(struct registers* regs);

#endif
