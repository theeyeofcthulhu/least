global _start
extern uprint

section .text
    mov rax, rax

    mov rax, [rbx]
    mov [rbx], rax
_start:
    mov dword [ebp], 0x1
    mov dword [eax], 0x1
    mov dword [rsp], 0x1

    mov rax, 1
    mov rdi, 1
    mov rsi, msg
    mov rdx, msg_len
    syscall

    mov rax, 60
    mov rdi, 2
    syscall

    div rax
    div rdx

    call uprint

    cmp rax, rdi
    je .sym1
    jmp .sym1

.sym1:
    mov rax, 0xffee22
    mov rax, 0x214365
    mov rax, -0x214365
    mov rax, -0x2143659788
    mov rcx, 0

section .rodata
msg: db "Hello, World", 0xa
msg_len: equ $ - msg
