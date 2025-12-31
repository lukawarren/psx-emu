#pragma once

class GTE
{
public:
    CycleCount execute(const u32 instruction);
    u32 read_register(const u32 reg);
    void write_register(const u32 reg, const u32 value);

private:
    // Data registers
    i16 v0[3];          // VXY0, VZ0
    i16 v1[3];          // VXY1, VZ1
    i16 v2[3];          // VXY2, VZ2
    u8 rgbc[4];         // Color / code value
    u16 otz;            // Average z value
    i16 ir0;            // 16-bit accumulator (interpolate)
    i16 ir1;            // 16-bit accumulator (vector)
    i16 ir2;            // 16-bit accumulator (vector)
    i16 ir3;            // 16-bit accumulator (vector)
    i16 sx[3];          // SX0, SX1, SX2 (and SXYP); 3 stages
    i16 sy[3];          // SY0, SY1, SY2 (and SXYP); 3 stages
    u16 sz[4];          // SZ0, SZ1, SZ2, SZ3; 4 stages
    u32 rgb[3];         // 3 stages
    u32 res1;           // Prohibited but needed to pass GTE tests
    i32 mac0;           // 32-bit maths accumulator (value)
    i32 mac1;           // 32-bit maths accumulator (vector)
    i32 mac2;           // 32-bit maths accumulator (vector)
    i32 mac3;           // 32-bit maths accumulator (vector)
    u16 irgb;           // For converting 48-bit vs 15-bit colour
    u16 orgb;           // For converting 48-bit vs 15-bit colour
    u32 lzcs;           // For counting leading zeroes and ones
    u32 lzcr;           // For counting leading zeroes and ones

    // Control registers
    i16 rt[3][3];       // Rotation matrix
    i32 tr[3];          // Translation vector
    i16 ll[3][3];       // Light source matrix
    u32 rbk;            // Background colour (r, g, b)
    u32 gbk;            // Background colour (r, g, b)
    u32 bbk;            // Background colour (r, g, b)
    i16 lc[3][3];       // Light colour matrix
    i32 rfc;            // Far colour (r, g, b)
    i32 gfc;            // Far colour (r, g, b)
    i32 bfc;            // Far colour (r, g, b)
    i32 ofx;            // Screen offset (x, y)
    i32 ofy;            // Screen offset (x, y)
    u16 h;              // Projection plane distance
    i16 dqa;            // Depth queuing parameter A
    i32 dqb;            // Depth queuing parameter B
    i16 zsf3;           // Average Z scale factors
    i16 zsf4;           // Average Z scale factors
    u32 flag;           // Flag register

    // Flags
    constexpr static u32 FLAG_ERROR = 1U << 31;
    constexpr static u32 FLAG_MAC0_OVERFLOW_POSITIVE = 1U << 16;
    constexpr static u32 FLAG_MAC1_OVERFLOW_POSITIVE = 1U << 30;
    constexpr static u32 FLAG_MAC2_OVERFLOW_POSITIVE = 1U << 29;
    constexpr static u32 FLAG_MAC3_OVERFLOW_POSITIVE = 1U << 28;
    constexpr static u32 FLAG_MAC1_OVERFLOW_NEGATIVE = 1U << 27;
    constexpr static u32 FLAG_MAC2_OVERFLOW_NEGATIVE = 1U << 26;
    constexpr static u32 FLAG_MAC3_OVERFLOW_NEGATIVE = 1U << 25;
    constexpr static u32 FLAG_MAC0_OVERFLOW_NEGATIVE = 1U << 15;
    constexpr static u32 FLAG_IR0_SATURATED = 1U << 12;
    constexpr static u32 FLAG_IR1_SATURATED = 1U << 24;
    constexpr static u32 FLAG_IR2_SATURATED = 1U << 23;
    constexpr static u32 FLAG_IR3_SATURATED = 1U << 22;
    constexpr static u32 FLAG_COLOR_R_SATURATED = 1U << 21;
    constexpr static u32 FLAG_COLOR_G_SATURATED = 1U << 20;
    constexpr static u32 FLAG_COLOR_B_SATURATED = 1U << 19;
    constexpr static u32 FLAG_SX2_SATURATED = 1U << 14;
    constexpr static u32 FLAG_SY2_SATURATED = 1U << 13;
    constexpr static u32 FLAG_SZ3_OTZ_SATURATED = 1U << 18;
    constexpr static u32 FLAG_DIVIDE_OVERFLOW = 1U << 17;

    inline bool get_shift_fraction(const u32 instruction)
    {
        return ((instruction >> 19) & 0b1) != 0;
    }

    inline bool get_limit(const u32 instruction)
    {
        return ((instruction >> 10) & 0b1) != 0;
    }

    i32 clip(
        const i32 value,
        const i32 max,
        const i32 min,
        const u32 flags
    );

    template <size_t bit_size, typename T = int16_t>
    T extend_sign(const i64 n) const;

    template <int bits>
    void check_overflow(
        const i64 value,
        const u32 overflow_flags,
        const u32 underflow_flags
    );

    template <int i>
    i64 check_mac_overflow_and_extend(const i64 value);

    template <int i>
    i64 set_mac(i64 value, const bool shift_fraction);

    inline i64 set_mac0(const i64 value);

    template <int i>
    void set_ir(const i32 value, const bool limit);

    void set_ir0(const i32 value);

    template <int i>
    void set_mac_and_ir(
        const i64 value,
        const bool shift_fraction,
        const bool limit
    );

    void push_screen_x_and_y(const i32 x, const i32 y);
    void push_screen_z(const i32 z);

    u32 divide_unsigned_newton_raphson(u32 lhs, u32 rhs);

    // Instructions
    CycleCount rtps (const u32 instruction);
    CycleCount nclip(const u32 instruction);
    CycleCount op   (const u32 instruction);
    CycleCount dpcs (const u32 instruction);
    CycleCount intpl(const u32 instruction);
    CycleCount mvmva(const u32 instruction);
    CycleCount ncds (const u32 instruction);
    CycleCount cdp  (const u32 instruction);
    CycleCount ncdt (const u32 instruction);
    CycleCount nccs (const u32 instruction);
    CycleCount cc   (const u32 instruction);
    CycleCount ncs  (const u32 instruction);
    CycleCount nct  (const u32 instruction);
    CycleCount sqr  (const u32 instruction);
    CycleCount dcpl (const u32 instruction);
    CycleCount dpct (const u32 instruction);
    CycleCount avsz3(const u32 instruction);
    CycleCount avsz4(const u32 instruction);
    CycleCount rtpt (const u32 instruction);
    CycleCount gpf  (const u32 instruction);
    CycleCount gpl  (const u32 instruction);
    CycleCount ncct (const u32 instruction);
};