

; Set up a multiboot header.
section .multiboot
align 4
    dd 0x1BADB002            ; Magic number
    dd 0x00                  ; Flags
    dd - (0x1BADB002 + 0x00) ; Checksum

; The stack size.
STACK_SIZE equ 0x4000

; The actual boot code.
[bits 32]
section .text
global _start
extern kmain

_start:
    ; Set up the stack.
    mov esp, stack_space + STACK_SIZE

    ; Call the C kernel's main function.
    call kmain

    ; Halt the CPU.
    cli
    hlt

; The stack.
section .bss
align 16
stack_space:
    resb STACK_SIZE

