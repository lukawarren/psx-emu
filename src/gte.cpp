#include "gte.h"

#define CMOAE(i, value) check_mac_overflow_and_extend<i>(value)

CycleCount GTE::execute(const u32 instruction)
{
    const auto opcode = instruction & 0b111111;

    switch (opcode)
    {
        case 0x01: return rtps(instruction);
        case 0x06: return nclip(instruction);
        case 0x0C: return op(instruction);
        case 0x10: return dpcs(instruction);
        case 0x11: return intpl(instruction);
        case 0x12: return mvmva(instruction);
        case 0x13: return ncds(instruction);
        case 0x14: return cdp(instruction);
        case 0x16: return ncdt(instruction);
        case 0x1B: return nccs(instruction);
        case 0x1C: return cc(instruction);
        case 0x1E: return ncs(instruction);
        case 0x20: return nct(instruction);
        case 0x28: return sqr(instruction);
        case 0x29: return dcpl(instruction);
        case 0x2A: return dpct(instruction);
        case 0x2D: return avsz3(instruction);
        case 0x2E: return avsz4(instruction);
        case 0x30: return rtpt(instruction);
        case 0x3D: return gpf(instruction);
        case 0x3E: return gpl(instruction);
        case 0x3F: return ncct(instruction);

        default:
            dbg("TODO: unknown GTE instruction", dbg::hex(opcode));
            exit(1);
    }

}

u32 GTE::read_register(const u32 reg)
{
    switch (reg & 0b111111)
    {
        case  0: return ((u32)v0[0] & 0xFFFF) | ((u32)v0[1] << 16);
        case  1: return (u32)(i32)v0[2];
        case  2: return (u32)(v1[0] & 0xFFFF) | ((u32)v1[1] << 16);
        case  3: return (u32)(i32)v1[2];
        case  4: return (u32)(v2[0] & 0xFFFF) | ((u32)v2[1] << 16);
        case  5: return (u32)(i32)v2[2];
        case  6: return ((u32)rgbc[3] << 24) |
                        ((u32)rgbc[2] << 16) |
                        ((u32)rgbc[1] <<  8) |
                        ((u32)rgbc[0] <<  0);
        case  7: return (u32)otz;
        case  8: return (u32)(i32)ir0;
        case  9: return (u32)(i32)ir1;
        case 10: return (u32)(i32)ir2;
        case 11: return (u32)(i32)ir3;
        case 12: return ((u16)sy[0] << 16) | (u16)sx[0];
        case 13: return ((u16)sy[1] << 16) | (u16)sx[1];
        case 14:
        case 15: return ((u16)sy[2] << 16) | (u16)sx[2];
        case 16: return (u16)sz[0];
        case 17: return (u16)sz[1];
        case 18: return (u16)sz[2];
        case 19: return (u16)sz[3];
        case 20: return rgb[0];
        case 21: return rgb[1];
        case 22: return rgb[2];
        case 23: return 0; // Unused
        case 24: return (u32)mac0;
        case 25: return (u32)mac1;
        case 26: return (u32)mac2;
        case 27: return (u32)mac3;
        case 28:
        case 29:
        {
            u32 irgb = 0;
            irgb |= (u32)clip(ir1 / 0x80, 0x1f, 0, 0);
            irgb |= (u32)(clip(ir2 / 0x80, 0x1f, 0, 0) << 5);
            irgb |= (u32)(clip(ir3 / 0x80, 0x1f, 0, 0) << 10);
            return irgb;
        }
        case 30: return lzcs;
        case 31: return lzcr;
        case 32: return ((u32)(u16)rt[0][1] << 16) | (u32)rt[0][0];
        case 33: return ((u32)(u16)rt[1][0] << 16) | (u32)rt[0][2];
        case 34: return ((u32)(u16)rt[1][2] << 16) | (u32)rt[1][1];
        case 35: return ((u32)(u16)rt[2][1] << 16) | (u32)rt[2][0];
        case 36: return (u32)(i32)(rt[2][2]);
        case 37: return (u32)tr[0];
        case 38: return (u32)tr[1];
        case 39: return (u32)tr[2];
        case 40: return ((u32)(u16)ll[0][1] << 16) | (u32)ll[0][0];
        case 41: return ((u32)(u16)ll[1][0] << 16) | (u32)ll[0][2];
        case 42: return ((u32)(u16)ll[1][2] << 16) | (u32)ll[1][1];
        case 43: return ((u32)(u16)ll[2][1] << 16) | (u32)ll[2][0];
        case 44: return (u32)(i32)(ll[2][2]);
        case 45: return rbk;
        case 46: return gbk;
        case 47: return bbk;
        case 48: return ((u32)(u16)lc[0][1] << 16) | (u32)lc[0][0];
        case 49: return ((u32)(u16)lc[1][0] << 16) | (u32)lc[0][2];
        case 50: return ((u32)(u16)lc[1][2] << 16) | (u32)lc[1][1];
        case 51: return ((u32)(u16)lc[2][1] << 16) | (u32)lc[2][0];
        case 52: return (u32)(i32)(lc[2][2]);
        case 53: return rfc;
        case 54: return gfc;
        case 55: return bfc;
        case 56: return ofx;
        case 57: return ofy;
        case 58: return (u32)(i32)((i16)h); // Hardware bug
        case 59: return (u32)(i32)(dqa);
        case 60: return (u32)dqb;
        case 61: return (u32)(i32)(zsf3);
        case 62: return (u32)(i32)(zsf4);
        case 63: return flag;
    }

    return 0;
}

void GTE::write_register(const u32 reg, const u32 value)
{
    switch (reg & 0b111111)
    {
        case  0: v0[0] = (i16)(u16)(value & 0xFFFF); v0[1] = (i16)(u16)(value >> 16); break;
        case  1: v0[2] = (i16)(u16)(value & 0xFFFF);                                  break;
        case  2: v1[0] = (i16)(u16)(value & 0xFFFF); v1[1] = (i16)(u16)(value >> 16); break;
        case  3: v1[2] = (i16)(u16)(value & 0xFFFF);                                  break;
        case  4: v2[0] = (i16)(u16)(value & 0xFFFF); v2[1] = (i16)(u16)(value >> 16); break;
        case  5: v2[2] = (i16)(u16)(value & 0xFFFF);                                  break;
        case  6:
            rgbc[0] = (u8)((value >>  0) & 0xFF);
            rgbc[1] = (u8)((value >>  4) & 0xFF);
            rgbc[2] = (u8)((value >>  8) & 0xFF);
            rgbc[3] = (u8)((value >> 12) & 0xFF);
            break;
        case  7: otz = (u16)value;                                                    break;
        case  8: ir0 = (i16)(u16)value;                                               break;
        case  9: ir1 = (i16)(u16)value;                                               break;
        case 10: ir2 = (i16)(u16)value;                                               break;
        case 11: ir3 = (i16)(u16)value;                                               break;
        case 12: sy[0] = (i16)(u16)(value >> 16); sx[0] = (i16)(u16)value;            break;
        case 13: sy[1] = (i16)(u16)(value >> 16); sx[1] = (i16)(u16)value;            break;
        case 14: sy[2] = (i16)(u16)(value >> 16); sx[2] = (i16)(u16)value;            break;
        case 15:
            sx[0] = sx[1];
            sy[0] = sy[1];
            sx[1] = sx[2];
            sy[1] = sy[2];
            sx[2] = (i16)(u16)value;
            sy[2] = (i16)(u16)(value >> 16);
            break;
        case 16: sz[0] = (u16)value;                                                  break;
        case 17: sz[1] = (u16)value;                                                  break;
        case 18: sz[2] = (u16)value;                                                  break;
        case 19: sz[3] = (u16)value;                                                  break;
        case 20: rgb[0] = value;                                                      break;
        case 21: rgb[1] = value;                                                      break;
        case 22: rgb[2] = value;                                                      break;
        case 23:                                                                      break; // Prohibited
        case 24: mac0 = (i64)(i32)value;                                              break;
        case 25: mac1 = (i64)(i32)value;                                              break;
        case 26: mac2 = (i64)(i32)value;                                              break;
        case 27: mac3 = (i64)(i32)value;                                              break;
        case 28:
            ir1 = (i16)(u16)((value & 0x1f) * 0x80);
            ir2 = (i16)(u16)(((value >> 5) & 0x1f) * 0x80);
            break;
        case 29:                                                                      break; // Read-only
        case 30: lzcs = value; lzcr = (u32)std::countl_zero(lzcs);                    break;
        case 31:                                                                      break; // Read-only
        case 32: rt[0][0] = (i16)(u16)value; rt[0][1] = (i16)(u16)(value >> 16);      break;
        case 33: rt[0][2] = (i16)(u16)value; rt[1][0] = (i16)(u16)(value >> 16);      break;
        case 34: rt[1][1] = (i16)(u16)value; rt[1][2] = (i16)(u16)(value >> 16);      break;
        case 35: rt[2][0] = (i16)(u16)value; rt[2][1] = (i16)(u16)(value >> 16);      break;
        case 36: rt[2][2] = (i16)(u16)value;                                          break;
        case 37: tr[0] = (i32)value;                                                  break;
        case 38: tr[1] = (i32)value;                                                  break;
        case 39: tr[2] = (i32)value;                                                  break;
        case 40: ll[0][0] = (i16)(u16)value; ll[0][1] = (i16)(u16)(value >> 16);      break;
        case 41: ll[0][2] = (i16)(u16)value; ll[1][0] = (i16)(u16)(value >> 16);      break;
        case 42: ll[1][1] = (i16)(u16)value; ll[1][2] = (i16)(u16)(value >> 16);      break;
        case 43: ll[2][0] = (i16)(u16)value; ll[2][1] = (i16)(u16)(value >> 16);      break;
        case 44: ll[2][2] = (i16)(u16)value;                                          break;
        case 45: rbk = value;                                                         break;
        case 46: gbk = value;                                                         break;
        case 47: bbk = value;                                                         break;
        case 48: lc[0][0] = (i16)(u16)value; lc[0][1] = (i16)(u16)(value >> 16);      break;
        case 49: lc[0][2] = (i16)(u16)value; lc[1][0] = (i16)(u16)(value >> 16);      break;
        case 50: lc[1][1] = (i16)(u16)value; lc[1][2] = (i16)(u16)(value >> 16);      break;
        case 51: lc[2][0] = (i16)(u16)value; lc[2][1] = (i16)(u16)(value >> 16);      break;
        case 52: lc[2][2] = (i16)(u16)value;                                          break;
        case 53: rfc = value;                                                         break;
        case 54: gfc = value;                                                         break;
        case 55: bfc = value;                                                         break;
        case 56: ofx = value;                                                         break;
        case 57: ofy = value;                                                         break;
        case 58: h = (u16)value;                                                      break;
        case 59: dqa = (i16)value;                                                    break;
        case 60: dqb = (i32)value;                                                    break;
        case 61: zsf3 = (i16)value;                                                   break;
        case 62: zsf4 = (i16)value;                                                   break;
        case 63: flag = value & 0x7FFFF000;                                           break;
    }
}

i32 GTE::clip(const i32 value, const i32 max, const i32 min, const u32 flags)
{
    if (value > max)
    {
        flag |= flags;
        return max;
    }

    if (value < min)
    {
        flag |= flags;
        return min;
    }
    return value;
}

template <size_t bit_size, typename T>
T GTE::extend_sign(const i64 n) const
{
    static_assert(bit_size > 0 && bit_size < 63);

    T mask = ((1LL << (bit_size - 1)) - 1);
    bool sign = (n & (1LL << (bit_size - 1))) != 0;

    T val = n & mask;
    if (sign) val |= ~mask;
    return val;
}

template <int bits>
void GTE::check_overflow(const i64 value, const u32 overflow_flags, const u32 underflow_flags)
{
    if (value >= (1LL << (bits - 1))) flag |= overflow_flags;
    if (value < -(1LL << (bits - 1))) flag |= underflow_flags;
}

template <int i>
i64 GTE::check_mac_overflow_and_extend(const i64 value)
{
    static_assert(i >= 1 && i <= 3);

    if constexpr (i == 1)
        check_overflow<44>(value, FLAG_MAC1_OVERFLOW_POSITIVE, FLAG_MAC1_OVERFLOW_NEGATIVE);
    if constexpr (i == 2)
        check_overflow<44>(value, FLAG_MAC2_OVERFLOW_POSITIVE, FLAG_MAC2_OVERFLOW_NEGATIVE);
    if constexpr (i == 3)
        check_overflow<44>(value, FLAG_MAC3_OVERFLOW_POSITIVE, FLAG_MAC3_OVERFLOW_NEGATIVE);

    return extend_sign<44, i64>(value);
}

template <int i>
i64 GTE::set_mac(i64 value, const bool shift_fraction)
{
    static_assert(i >= 1 && i <= 3);

    // Check for overflow; set flags accordingly
    if constexpr (i == 1)
        check_overflow<44>(value, FLAG_MAC1_OVERFLOW_POSITIVE, FLAG_MAC1_OVERFLOW_NEGATIVE);
    if constexpr (i == 2)
        check_overflow<44>(value, FLAG_MAC2_OVERFLOW_POSITIVE, FLAG_MAC2_OVERFLOW_NEGATIVE);
    if constexpr (i == 3)
        check_overflow<44>(value, FLAG_MAC3_OVERFLOW_POSITIVE, FLAG_MAC3_OVERFLOW_NEGATIVE);

    // Remove fractional part if need be
    if (shift_fraction)
        value >>= 12;

    // Write to MAC register; return value written
    if constexpr (i == 1) mac1 = (i32)value;
    if constexpr (i == 2) mac2 = (i32)value;
    if constexpr (i == 3) mac3 = (i32)value;
    return value;
}

inline i64 GTE::set_mac0(const i64 value)
{
    check_overflow<32>(value, FLAG_MAC0_OVERFLOW_POSITIVE, FLAG_MAC0_OVERFLOW_POSITIVE);
	mac0 = (i32)value;
	return value;
}


template <int i>
void GTE::set_ir(const i32 value, const bool limit)
{
    static_assert(i >= 1 && i <= 3);

    u32 saturated_bits = 0;
    if constexpr (i == 1) saturated_bits = FLAG_IR1_SATURATED;
    if constexpr (i == 2) saturated_bits = FLAG_IR2_SATURATED;
    if constexpr (i == 3) saturated_bits = FLAG_IR3_SATURATED;

    const i32 clipped = clip(value, 0x7fff, limit ? 0 : -0x8000, saturated_bits);
    if constexpr (i == 1) ir1 = (i16)clipped;
    if constexpr (i == 2) ir2 = (i16)clipped;
    if constexpr (i == 3) ir3 = (i16)clipped;
}

void GTE::set_ir0(const i32 value)
{
    ir0 = (i16)clip(value, std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max(), FLAG_IR0_SATURATED);
}

template <int i>
void GTE::set_mac_and_ir(const i64 value, const bool shift_fraction, const bool limit)
{
    set_ir<i>((i32)set_mac<i>(value, shift_fraction), limit);
}

void GTE::push_screen_x_and_y(const i32 x, const i32 y)
{
    sx[0] = sx[1];
    sy[0] = sy[1];
    sx[1] = sx[2];
    sy[1] = sy[2];
    sx[2] = (i16)clip(x, 0x3ff, -0x400, FLAG_SX2_SATURATED);
    sy[2] = (i16)clip(y, 0x3ff, -0x400, FLAG_SY2_SATURATED);
}

void GTE::push_screen_z(i32 z)
{
    sz[0] = sz[1];
    sz[1] = sz[2];
    sz[2] = sz[3];
    sz[3] = (u16)clip(z, 0xffff, 0x0000, FLAG_SZ3_OTZ_SATURATED);
}

u32 GTE::divide_unsigned_newton_raphson(u32 lhs, u32 rhs)
{
    if (lhs < rhs * 2)
    {
        static const std::array<uint8_t, 257> table
        {
            0xFF,0xFD,0xFB,0xF9,0xF7,0xF5,0xF3,0xF1,0xEF,0xEE,0xEC,0xEA,0xE8,0xE6,0xE4,0xE3,
            0xE1,0xDF,0xDD,0xDC,0xDA,0xD8,0xD6,0xD5,0xD3,0xD1,0xD0,0xCE,0xCD,0xCB,0xC9,0xC8,
            0xC6,0xC5,0xC3,0xC1,0xC0,0xBE,0xBD,0xBB,0xBA,0xB8,0xB7,0xB5,0xB4,0xB2,0xB1,0xB0,
            0xAE,0xAD,0xAB,0xAA,0xA9,0xA7,0xA6,0xA4,0xA3,0xA2,0xA0,0x9F,0x9E,0x9C,0x9B,0x9A,
            0x99,0x97,0x96,0x95,0x94,0x92,0x91,0x90,0x8F,0x8D,0x8C,0x8B,0x8A,0x89,0x87,0x86,
            0x85,0x84,0x83,0x82,0x81,0x7F,0x7E,0x7D,0x7C,0x7B,0x7A,0x79,0x78,0x77,0x75,0x74,
            0x73,0x72,0x71,0x70,0x6F,0x6E,0x6D,0x6C,0x6B,0x6A,0x69,0x68,0x67,0x66,0x65,0x64,
            0x63,0x62,0x61,0x60,0x5F,0x5E,0x5D,0x5D,0x5C,0x5B,0x5A,0x59,0x58,0x57,0x56,0x55,
            0x54,0x53,0x53,0x52,0x51,0x50,0x4F,0x4E,0x4D,0x4D,0x4C,0x4B,0x4A,0x49,0x48,0x48,
            0x47,0x46,0x45,0x44,0x43,0x43,0x42,0x41,0x40,0x3F,0x3F,0x3E,0x3D,0x3C,0x3C,0x3B,
            0x3A,0x39,0x39,0x38,0x37,0x36,0x36,0x35,0x34,0x33,0x33,0x32,0x31,0x31,0x30,0x2F,
            0x2E,0x2E,0x2D,0x2C,0x2C,0x2B,0x2A,0x2A,0x29,0x28,0x28,0x27,0x26,0x26,0x25,0x24,
            0x24,0x23,0x22,0x22,0x21,0x20,0x20,0x1F,0x1E,0x1E,0x1D,0x1D,0x1C,0x1B,0x1B,0x1A,
            0x19,0x19,0x18,0x18,0x17,0x16,0x16,0x15,0x15,0x14,0x14,0x13,0x12,0x12,0x11,0x11,
            0x10,0x0F,0x0F,0x0E,0x0E,0x0D,0x0D,0x0C,0x0C,0x0B,0x0A,0x0A,0x09,0x09,0x08,0x08,
            0x07,0x07,0x06,0x06,0x05,0x05,0x04,0x04,0x03,0x03,0x02,0x02,0x01,0x01,0x00,0x00,
            0x00
        };

        const int z = std::countl_zero<u16>((u16)rhs);
        const u32 n = lhs << z;
        u32 d = rhs << z;
        const u32 index = (d - 0x7fc0) >> 7;
        const u32 u = table[index] + 0x101;
        d = (u32)((0x2000080 - (u64)d * (u64)u) >> 8);
        d = (u32)((0x80 + (u64)d * (u64)u) >> 8);

        return std::min<u32>(0x1ffff, (u32)(((u64)n * (u64)d + 0x8000) >> 16));
    }
    else
    {
        flag |= FLAG_DIVIDE_OVERFLOW;
        flag |= FLAG_ERROR;
        return 0x1ffff;
    }
}

// Perspective transformation (single)
CycleCount GTE::rtps(const u32 instruction)
{
    const bool sf = get_shift_fraction(instruction);
    const bool lm = get_limit(instruction);

    // TR + RT * V0
    int64_t result[3];
    result[0] = CMOAE(1, CMOAE(1, CMOAE(1, ((int64_t)tr[0] << 12) + rt[0][0] * v0[0]) + rt[0][1] * v0[1]) + rt[0][2] * v0[2]);
    result[1] = CMOAE(2, CMOAE(2, CMOAE(2, ((int64_t)tr[1] << 12) + rt[1][0] * v0[0]) + rt[1][1] * v0[1]) + rt[1][2] * v0[2]);
    result[2] = CMOAE(3, CMOAE(3, CMOAE(3, ((int64_t)tr[2] << 12) + rt[2][0] * v0[0]) + rt[2][1] * v0[1]) + rt[2][2] * v0[2]);

    // Write to IR (save for 3) and MAC
    set_mac_and_ir<1>(result[0], sf, lm);
    set_mac_and_ir<2>(result[1], sf, lm);
    set_mac<3>(result[2], sf);

    // When RTP with sf=0, IR3 saturation flag gets set only if "MAC3 SAR 12" exceeds -8000h..+7FFFh
    clip((i32)(result[2] >> 12), 0x7fff, -0x8000, FLAG_IR3_SATURATED);

    // Calculation still respects lm bit though; write to IR3 now
    ir3 = (i16)clip(mac3, 0x7fff, lm ? 0 : -0x8000, 0);

    // Push to SZ3
    push_screen_z((i32)(result[2] >> 12));

    // Perform division; calculate ((H * 0x20000 / SZ3) + 1) / 2
	const i64 unr_result = divide_unsigned_newton_raphson(h, sz[3]);

    const i32 x = (i32)set_mac0(unr_result * ir1 + ofx) >> 16;
    const i32 y = (i32)set_mac0(unr_result * ir2 + ofy) >> 16;
    push_screen_x_and_y(x, y);

    const i64 mac0 = set_mac0(unr_result * (i64)dqa + (i64)dqb);
    ir0 = (i16)clip((i32)(mac0 >> 12), 0x1000, 0x0000, FLAG_IR0_SATURATED);

    return 15;
}

CycleCount GTE::nclip(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::op(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::dpcs(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::intpl(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }

// Multiply vector by matrix and add vector
CycleCount GTE::mvmva(const u32 instruction)
{
    i16 mx[3][3];
    i16 vx[3];
    i32 tx[3];

    const u32 matrix = (instruction >> 17) & 0b11;
    switch (matrix)
    {
        case 0: memcpy(mx, rt, sizeof(mx)); break;
        case 1: memcpy(mx, ll, sizeof(mx)); break;
        case 2: memcpy(mx, lc, sizeof(mx)); break;
        case 3: dbg("TODO"); exit(1);
    }

    const u32 vector = (instruction >> 15) & 0b11;
    switch (vector)
    {
        case 0: memcpy(vx, v0, sizeof(vx)); break;
        case 1: memcpy(vx, v1, sizeof(vx)); break;
        case 2: memcpy(vx, v2, sizeof(vx)); break;
        case 3: dbg("TODO"); exit(1);
    }

    const u32 translation = (instruction >> 13) & 0b11;
    switch (translation)
    {
        case 0: memcpy(tx, tr, sizeof(tx)); break;
        case 1: dbg("TODO"); exit(1); break;
        case 2: dbg("TODO"); exit(1); break;
        case 3: memset(tx, 0, sizeof(tx)); break;
    }

    // TX + MX * VX
    i64 new_mac_1 = CMOAE(1, CMOAE(1, CMOAE(1, ((i64)tx[0] << 12) + (i64)mx[0][0] * vx[0]) + (i64)mx[0][1] * vx[1]) + i64(mx[0][2]) * vx[2]);
    i64 new_mac_2 = CMOAE(2, CMOAE(2, CMOAE(2, ((i64)tx[1] << 12) + (i64)mx[1][0] * vx[0]) + (i64)mx[1][1] * vx[1]) + i64(mx[1][2]) * vx[2]);
    i64 new_mac_3 = CMOAE(3, CMOAE(3, CMOAE(3, ((i64)tx[2] << 12) + (i64)mx[2][0] * vx[0]) + (i64)mx[2][1] * vx[1]) + i64(mx[2][2]) * vx[2]);

    const bool sf = get_shift_fraction(instruction);
    const bool lm = get_limit(instruction);
    set_mac_and_ir<1>(new_mac_1, sf, lm);
    set_mac_and_ir<2>(new_mac_2, sf, lm);
    set_mac_and_ir<3>(new_mac_3, sf, lm);

    return 8;
}

CycleCount GTE::ncds(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::cdp(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::ncdt(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::nccs(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::cc(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::ncs(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::nct(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::sqr(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::dcpl(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::dpct(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::avsz3(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; };
CycleCount GTE::avsz4(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; };
CycleCount GTE::rtpt(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::gpf(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::gpl(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::ncct(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }