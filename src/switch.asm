global switch_to_user_mode
global context_switch
global enter_usermode

; void enter_usermode(uint32_t entry, uint32_t stack);
enter_usermode:
    cli
    
    mov ax, 0x23      ; User Data Segment (Index 4 | RPL 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, [esp + 8]  ; Get stack argument
    push 0x23           ; SS (User Data Segment)
    push eax            ; ESP
    pushf               ; EFLAGS
    pop eax
    or eax, 0x200       ; Enable Interrupts (IF)
    push eax
    push 0x1B           ; CS (User Code Segment: Index 3 | RPL 3)
    mov eax, [esp + 20] ; Get entry argument (adjusted for pushes)
    push eax            ; EIP
    iret

; void context_switch(uint32_t* old_esp, uint32_t new_esp);
context_switch:
    ; Save previous context
    push ebx
    push esi
    push edi
    push ebp

    ; Save old ESP
    mov eax, [esp + 20]   ; Get old_esp pointer (4 saved regs + ret addr = 20 bytes offset)
    mov [eax], esp

    ; Load new ESP
    mov esp, [esp + 24]   ; Get new_esp value

    ; Restore new context
    pop ebp
    pop edi
    pop esi
    pop ebx

    ret

switch_to_user_mode:
    cli
    mov ax, 0x23      ; User Data Segment (Index 4 | RPL 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp
    push 0x23         ; SS (User Data Segment)
    push eax          ; ESP
    pushf             ; EFLAGS
    pop eax
    or eax, 0x200     ; Enable Interrupts (IF)
    push eax
    push 0x1B         ; CS (User Code Segment: Index 3 | RPL 3)
    push user_mode_entry_point ; EIP
    iret

user_mode_entry_point:
    ; We are now in user mode (Ring 3)!
    ; For now, just infinite loop to verify the switch worked
    ; TODO: Set up proper user space with mapped memory for syscalls
    jmp $

