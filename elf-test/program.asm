global _start

section .text
_start:
    mov rax, 1
    mov rdi, 1
    mov rsi, msg2
    mov rsi, msg
    mov rdx, msg_len
    syscall

    mov rax, 60
    mov rdi, 2
    syscall

.sym1:
    mov rax, 0xffee22
    mov rax, 0x214365
    mov rax, -0x214365
    mov rax, -0x2143659788
    mov rcx, 0

section .rodata
msg: db "Hello, World", 0xa
msg2: db "Hello, World", 0xa
msg_len: equ $ - msg
