section .text
extern putchar

; void putchar(char c);
putchar:
    enter 1,0
    mov byte [rsp], dil
    mov rsi, rsp
    mov rax, 1
    mov rdi, 1
    mov rdx, 1
    syscall
    leave
    ret
