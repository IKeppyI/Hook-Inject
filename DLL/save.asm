.data
extern trampoline:QWORD
extern callback:QWORD
.code



saver PROC
    push rdi    
    push rsi    
    push rbx   
    push rbp
    push r12    
    push r13    
    push r14    
    push r15
    push r8
    push r9
    push r10
    push r11
    push rdx
    push rcx
    push rax
   
    mov rax, callback   
    call rax 
    
    pop rax
    pop rcx
    pop rdx
    pop r11
    pop r10
    pop r9
    pop r8
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop rsi    
    pop rdi
    
    mov rax, trampoline
    jmp rax

saver ENDP
END
