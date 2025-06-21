/* rtl8139-stage0/net_tiny.h */
#pragma once
#include <stdint.h>

void     net_init(const uint8_t mac[6]);
int      tftp_fetch(const char *fname,
                    uint8_t *dst, uint32_t max_len);
