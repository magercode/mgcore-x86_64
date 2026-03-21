[bits 64]

extern kernel_main
extern boot_stack_top
extern multiboot_magic
extern multiboot_info_ptr

global long_mode_start

section .text.boot
long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov rsp, boot_stack_top
    xor rbp, rbp

    mov edi, dword [rel multiboot_magic]
    mov esi, dword [rel multiboot_info_ptr]
    call kernel_main

.halt:
    cli
    hlt
    jmp .halt
