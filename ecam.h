/* rtl8139-stage0/ecam.h */
#pragma once
#include <stdint.h>

static inline volatile uint32_t *ecam_ptr(uint8_t bus, uint8_t dev,
                                          uint8_t fn, uint16_t off)
{
    return (volatile uint32_t *)(0xE0000000UL |
            ((uint32_t)bus << 20) | (dev << 15) |
            (fn << 12) | (off & 0xFFC));
}

static inline uint32_t ecam_r32(uint8_t b,uint8_t d,uint8_t f,uint16_t o)
{ return *ecam_ptr(b,d,f,o); }

static inline void ecam_w32(uint8_t b,uint8_t d,uint8_t f,
                            uint16_t o,uint32_t v)
{ *ecam_ptr(b,d,f,o)=v; }
