[bits 32]

%define MULTIBOOT2_HEADER_MAGIC 0xE85250D6
%define MULTIBOOT2_ARCHITECTURE 0
%define MULTIBOOT2_HEADER_LENGTH 24
%define MULTIBOOT2_CHECKSUM -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT2_ARCHITECTURE + MULTIBOOT2_HEADER_LENGTH)

%define CR4_PAE (1 << 5)
%define CR0_PG  (1 << 31)
%define IA32_EFER 0xC0000080
%define EFER_LME (1 << 8)
%define PAGE_PRESENT 1
%define PAGE_WRITE   2
%define PAGE_LARGE   (1 << 7)

extern long_mode_start

global multiboot_entry
global gdt64_pointer
global boot_stack_top
global multiboot_magic
global multiboot_info_ptr

section .multiboot
align 8
mb2_header_start:
    dd MULTIBOOT2_HEADER_MAGIC
    dd MULTIBOOT2_ARCHITECTURE
    dd MULTIBOOT2_HEADER_LENGTH
    dd MULTIBOOT2_CHECKSUM
    dw 0
    dw 0
    dd 8
mb2_header_end:

section .text.boot
multiboot_entry:
    cli
    mov esp, boot_stack_top
    mov [multiboot_magic], eax
    mov [multiboot_info_ptr], ebx

    call check_cpuid
    call check_long_mode
    call setup_page_tables
    call enable_long_mode

    lgdt [gdt64_pointer]
    jmp 0x08:long_mode_start

halt_forever:
    cli
    hlt
    jmp halt_forever

check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    jz halt_forever
    ret

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb halt_forever
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz halt_forever
    ret

setup_page_tables:
    mov eax, pdpt_low
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [pml4_table], eax

    mov eax, pd_low
    or eax, PAGE_PRESENT | PAGE_WRITE
    mov [pdpt_low], eax

    xor ecx, ecx
.map_2m:
    mov eax, ecx
    shl eax, 21
    or eax, PAGE_PRESENT | PAGE_WRITE | PAGE_LARGE
    mov [pd_low + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_2m
    ret

enable_long_mode:
    mov eax, cr4
    or eax, CR4_PAE
    mov cr4, eax

    mov eax, pml4_table
    mov cr3, eax

    mov ecx, IA32_EFER
    rdmsr
    or eax, EFER_LME
    wrmsr

    mov eax, cr0
    or eax, CR0_PG
    mov cr0, eax
    ret

section .rodata
align 8
gdt64:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF
    dq 0x00AF92000000FFFF
    dq 0x00AFFA000000FFFF
    dq 0x00AFF2000000FFFF
gdt64_pointer:
    dw gdt64_pointer - gdt64 - 1
    dq gdt64

section .bss
align 16
multiboot_magic:   resd 1
multiboot_info_ptr: resd 1
boot_stack_bottom: resb 16384
boot_stack_top:

section .bss.page_tables nobits align=4096
pml4_table: resq 512
pdpt_low:   resq 512
pd_low:     resq 512
