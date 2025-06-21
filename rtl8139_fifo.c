/* rtl8139-stage0/rtl8139_fifo.c : polled FIFO-only RTL8139 */
#include "rtl8139_fifo.h"
#include "ecam.h"

#define VIRT(addr) ((volatile uint8_t *)((uintptr_t)mm + (addr)))

/* --- registers --- */
#define IDR0    0x00
#define CR      0x37
#define IMR     0x3C
#define ISR     0x3E
#define RCR     0x44
#define CAPR    0x38
#define TPSR    0x20
#define TCR     0x40

/* CR bits */
#define CR_RESET (1<<4)
#define CR_RE    (1<<3)
#define CR_TE    (1<<2)

/* RCR bits */
#define RCR_AB   (1<<4)
#define RCR_AM   (1<<3)
#define RCR_APM  (1<<7)

/* FIFO size */
#define RX_FIFO_SIZE (64*1024)

static volatile uint8_t *mm;         /* BAR1 virtual base */

/* ---------- tiny memcpy (no libc) ---------- */
static void mem_cpy(void *dst, const void *src, uint16_t n)
{
    uint8_t *d = dst; const uint8_t *s = src;
    while (n--) *d++ = *s++;
}

/* ---------- probe & init ---------- */
int rtl8139_init(uint32_t *bar_out, uint8_t mac[6])
{
    for (uint8_t bus=0; bus<1; ++bus)
        for (uint8_t dev=0; dev<32; ++dev) {
            uint32_t id = ecam_r32(bus,dev,0,0x00);
            if (id == 0x813910EC) {              /* 10EC:8139 */
                uint32_t bar1 = ecam_r32(bus,dev,0,0x14);
                ecam_w32(bus,dev,0,0x04,
                         ecam_r32(bus,dev,0,0x04) | 2); /* MEM enable */

                *bar_out = bar1 & ~0xF;
                mm = (volatile uint8_t *)(uintptr_t)(*bar_out);

                /* Soft reset */
                VIRT(CR)[0] = CR_RESET;
                while (VIRT(CR)[0] & CR_RESET) ;

                /* fetch MAC */
                for (int i=0;i<6;i++) mac[i] = VIRT(IDR0)[i];

                /* Rx config */
                *(volatile uint32_t *)VIRT(RCR) =
                    RCR_AB | RCR_AM | RCR_APM;

                /* Clear ints */
                *(volatile uint16_t *)VIRT(IMR) = 0;
                *(volatile uint16_t *)VIRT(ISR) = 0xFFFF;

                /* Enable Rx/Tx */
                VIRT(CR)[0] |= CR_RE;
                return 0;
            }
        }
    return -1;
}

/* ---------- read one frame ---------- */
int rtl8139_read(uint8_t *dst, uint16_t *len_out)
{
    uint16_t isr = *(volatile uint16_t *)VIRT(ISR);
    if (!(isr & 0x01)) return 0;               /* no ROK */

    static uint16_t rx_off = 0;                /* CAPR pointer */

    uint16_t status = *(volatile uint16_t *)(mm + rx_off);
    uint16_t len    = *(volatile uint16_t *)(mm + rx_off + 2);
    uint16_t ptr    = rx_off + 4;

    for (uint16_t i=0;i<len;i++,ptr++)
        dst[i] = *(volatile uint8_t *)(mm + (ptr & (RX_FIFO_SIZE-1)));

    rx_off = (ptr + 3) & ~3;                   /* DWORD align */
    *(volatile uint16_t *)VIRT(CAPR) = rx_off - 0x10;
    *(volatile uint16_t *)VIRT(ISR)  = 0x01;

    (void)status;
    *len_out = len;
    return 1;
}

/* ---------- send (≤ 1514 B) via Tx FIFO window ---------- */
int rtl8139_send(const uint8_t *pkt, uint16_t len)
{
    if (len > 1514) return -1;
    /* Tx FIFO window 0x20-0x3F (32 dwords) */
    mem_cpy(VIRT(TPSR), pkt, len);
    *(volatile uint16_t *)VIRT(TCR) = len;
    /* Wait transmit complete (TR_OK in ISR bit 2) */
    while (!(*(volatile uint16_t *)VIRT(ISR) & (1<<2))) ;
    *(volatile uint16_t *)VIRT(ISR) = (1<<2);
    return 0;
}
