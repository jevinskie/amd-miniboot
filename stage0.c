/* rtl8139-stage0/stage0.c */
#include "rtl8139_fifo.h"
#include "net_tiny.h"

extern uint8_t _bss_end[];   /* provided by linker */

static void zero_bss(void)
{
    extern uint8_t __bss_start[];
    for (uint8_t *p=__bss_start; p<_bss_end; ++p) *p=0;
}

/* tiny memset/memcpy already in net_tiny */

static void multiboot_jump(uint32_t entry)
{
    asm volatile (
        "movl $0x2BADB002, %%eax\n\t"
        "xorl %%ebx, %%ebx\n\t"
        "jmp *%0"
        :: "r"(entry) : "eax"
    );
}

void stage0_main(void)
{
    zero_bss();

    uint32_t bar; uint8_t mac[6];
    if (rtl8139_init(&bar,mac)) for(;;);

    net_init(mac);

    if (tftp_fetch("boot.img",(uint8_t*)0x00100000,4*1024*1024))
        for(;;);

    multiboot_jump(0x00100000);
}
