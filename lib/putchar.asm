section .text
extern putchar
putchar:
    enter 1,0
    mov byte [rsp], al
    mov rsi, rsp
    mov rax, 1
    mov rdi, 1
    mov rdx, 1
    syscall
    leave
    ret
