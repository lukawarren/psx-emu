#pragma once
#include "common.h"

class Bus
{
public:
    u8 read_u8(const u32 address) { (void)address; return  0; }
    u16 read_u16(const u32 address) { (void)address; return 0; }
    u32 read_u32(const u32 address) { (void)address; return 0; }
    void write_u8(const u32 address, const u8 value) { (void)address; (void)value; }
    void write_u16(const u32 address, const u16 value) { (void)address; (void)value; }
    void write_u32(const u32 address, const u32 value) { (void)address; (void)value; }
};