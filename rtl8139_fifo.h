/* rtl8139-stage0/rtl8139_fifo.h */
#pragma once
#include <stdint.h>

int  rtl8139_init(uint32_t *bar_out, uint8_t mac[6]);
int  rtl8139_read(uint8_t *dst, uint16_t *len);
int  rtl8139_send(const uint8_t *pkt, uint16_t len);
