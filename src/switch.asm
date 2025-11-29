[bits 32]

; Context switch function
; void context_switch(uint32_t* old_esp, uint32_t new_esp)
;
; Saves current context to old_esp, loads new context from new_esp
; Uses callee-saved registers: EBX, ESI, EDI, EBP
; EIP is saved via the call/ret mechanism

global context_switch
context_switch:
    ; Get arguments
    mov eax, [esp + 4]    ; old_esp pointer
    mov edx, [esp + 8]    ; new_esp value

    ; Save callee-saved registers
    push ebp
    push ebx
    push esi
    push edi

    ; Save current stack pointer
    mov [eax], esp

    ; Load new stack pointer
    mov esp, edx

    ; Restore callee-saved registers
    pop edi
    pop esi
    pop ebx
    pop ebp

    ; Return to new process
    ; The 'ret' will pop the EIP that was pushed when this process
    ; previously called context_switch (or the initial entry point)
    ret
