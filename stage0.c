#include <stdint.h>
#include "rtl8139_fifo.h"
#include "net_tiny.h"

extern uint8_t __bss_start[], _bss_end[];

static void zero_bss(void) {
    for (uint8_t *p = __bss_start; p < _bss_end; ++p)
        *p = 0;
}

static void multiboot_jump(uint64_t entry) {
    __asm__ __volatile__ (
        "mov    $0x2BADB002, %%eax \n\t"
        "xor    %%ebx, %%ebx       \n\t"
        "jmp    *%0                \n\t"
        :
        : "r"(entry)
        : "eax", "ebx"
    );
}

void stage0_main(void) {
    zero_bss();

    uint32_t bar;
    uint8_t mac[6];
    if (rtl8139_init(&bar, mac))
        for (;;);

    net_init(mac);

    if (tftp_fetch("boot.img", (uint8_t *)0x00100000, 4 * 1024 * 1024))
        for (;;);

    multiboot_jump(0x00100000);
}
