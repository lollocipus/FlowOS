
[bits 32]

; Multiboot header
section .multiboot
align 4
    dd 0x1BADB002            ; Magic number
    dd 0x03                  ; Flags
    dd - (0x1BADB002 + 0x03) ; Checksum

; Stack size
STACK_SIZE equ 0x4000

section .text
global _start
extern kmain

_start:
    ; Set up the stack
    mov esp, stack_space + STACK_SIZE

    ; Clear interrupts until we set up IDT
    cli

    ; Push multiboot info pointer (ebx contains it from bootloader)
    push ebx

    ; Call the C kernel's main function
    call kmain
    
    ; Pop argument
    add esp, 4

    ; Halt if kmain returns
.halt:
    cli
    hlt
    jmp .halt

; Stack
section .bss
align 16
stack_space:
    resb STACK_SIZE

