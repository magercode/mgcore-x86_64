[bits 64]

global _start
extern main

section .text
_start:
    xor rbp, rbp
    mov rdi, [rsp]
    lea rsi, [rsp + 8]
    lea rdx, [rsi + rdi * 8 + 8]
    call main
    mov rdi, rax
    mov rax, 60
    syscall
.hang:
    hlt
    jmp .hang
