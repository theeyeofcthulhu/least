section .text

extern fprint

extern uprint
extern putchar

; void fprint(double f);
fprint:
    cvttsd2si r8, xmm0 ; rax = (int) xmm0

    mov rdi, r8
    call uprint ; print integer part of number

    mov rdi, 46
    call putchar ; putchar('.')

    ; xmm2 = f - (int) f
    movsd xmm2, xmm0
    cvtsi2sd xmm1, r8
    subsd xmm2, xmm1

    ; for (int i = 0; i < 6; i++)
    mov r9, 0
.loop:
    mulsd xmm2, [ten] ; xmm2 *= 10

    cvttsd2si rdi, xmm2 ; rdi = (int) f
    mov r8, rdi

    or dil, '0'
    call putchar

    ; xmm2 = f - (int) f
    cvtsi2sd xmm0, r8
    subsd xmm2, xmm0

    inc r9
    cmp r9, 6
    jl .loop

    ret

section .data
ten: dq 10.0
