/* rtl8139-stage0/entry64.S : 32-bit reset → 64-bit long mode */

.intel_syntax noprefix
.global _start

.macro  lgdt64 ptr
    mov     eax, offset \ptr
    lgdt    [eax]
.endm

/* Tiny 3-entry GDT in .rodata */
.section .rodata
gdt64:
    .quad 0x0000000000000000  /* null  */
    .quad 0x00AF9A000000FFFF /* 64-bit code */
    .quad 0x00AF92000000FFFF /* 64-bit data */
gdt64_ptr:
    .word  (gdt64_end - gdt64 - 1)
    .quad  gdt64
gdt64_end:

.text
_start:
    cli
    lgdt64 gdt64_ptr

    /* enable PE */
    mov     eax, cr0
    or      eax, 1
    mov     cr0, eax
    jmp     0x08:pm32

pm32:
    /* DS/SS → data selector (0x10) */
    mov     ax, 0x10
    mov     ds, ax
    mov     ss, ax
    mov     esp, 0x00088000             /* CAR stack */

    /* enable PAE */
    mov     eax, cr4
    or      eax, 1<<5
    mov     cr4, eax

    /* enable LME */
    mov     ecx, 0xC0000080
    rdmsr
    or      eax, 1<<8
    wrmsr

    /* Build minimal 4-MiB identity map in CAR */
    lea     esi, [setup_paging]
    call    esi

    /* turn on paging */
    mov     eax, cr0
    or      eax, 1<<31
    mov     cr0, eax
    jmp     0x08:pm64_entry

/* ---------- paging ---------- */
.section .text
setup_paging:
    /* PML4, PDPT, PD at 0x00087000.. */
    mov     edi, 0x00087000
    xor     eax, eax
    mov     ecx, 4096*3/4
    rep stosd

    /* PML4[0] → PDPT */
    mov     qword ptr [0x00087000], 0x00087103
    /* PDPT[0] → PD 4-MiB */
    mov     qword ptr [0x00087100], 0x00087203
    /* PD large page 0..4 MiB, RW, P */
    mov     dword ptr [0x00087200], 0x00000083

    /* load PML4 */
    mov     eax, 0x00087000
    mov     cr3, eax
    ret

/* ---------- long mode C entry ---------- */
pm64_entry:
    mov     rsp, 0x00088000
    extern  stage0_main
    call    stage0_main
    hlt
    jmp     .

