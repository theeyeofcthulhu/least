section .text
extern uprint

; void uprint(unsigned int n);
uprint:
	enter 32,0
	mov rax, rdi
	mov rdi, rbp
	mov rcx, 10

.div:
	xor rdx, rdx
	div rcx
	or dl, '0'
	dec rdi
	mov [rdi], dl
	test rax, rax
	jnz .div

	mov rsi, rdi
	mov rax, 1
	mov rdi, 1
	mov rdx, rbp
	sub rdx, rsi
	syscall
	
	leave
	ret
