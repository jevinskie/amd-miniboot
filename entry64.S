.intel_syntax noprefix
.global _start
.extern stage0_main

.section .text
.code16
_start:
    cli

    xor     ax, ax
    mov     ds, ax

    /* setup temporary GDT descriptor in low memory */
    lea     si, [gdt64_ptr]
    lgdt    [si]

    /* enable PE */
    mov     eax, cr0
    or      eax, 1
    mov     cr0, eax
    ljmp    0x08, pm32

.code32
pm32:
    mov     ax, 0x10
    mov     ds, ax
    mov     ss, ax
    mov     esp, 0x00088000

    mov     eax, cr4
    or      eax, 1 << 5
    mov     cr4, eax

    mov     ecx, 0xC0000080
    rdmsr
    or      eax, 1 << 8
    wrmsr

    call    setup_paging

    mov     eax, cr0
    or      eax, 1 << 31
    mov     cr0, eax
    ljmp    0x08, pm64

setup_paging:
    xor     eax, eax
    mov     edi, 0x00087000
    mov     ecx, (4096 * 3) / 4
    rep     stosd

    mov     dword ptr [0x00087000], 0x00087103
    mov     dword ptr [0x00087100], 0x00087203
    mov     dword ptr [0x00087200], 0x00000083

    mov     eax, 0x00087000
    mov     cr3, eax
    ret

.code64
pm64:
    mov     ax, 0x10
    mov     ds, ax
    mov     ss, ax
    mov     rsp, 0x00088000
    call    stage0_main
.hang:
    hlt
    jmp .hang

.section .text
gdt64:
    .quad 0x0000000000000000       /* null */
    .quad 0x00AF9A0000000000       /* code */
    .quad 0x00AF920000000000       /* data */

gdt64_ptr:
    .word gdt64_end - gdt64 - 1    /* limit */
    .long gdt64                    /* base low 32 bits */
    .word 0x0000                   /* base high (unused in 32-bit lgdt) */

gdt64_end:

.section .note.GNU-stack,"",@progbits

