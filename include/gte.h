#pragma once

class GTE
{
public:
    CycleCount execute(const u32 instruction);
    u32 read_data_register(const u32 reg);
    void write_data_register(const u32 reg, const u32 value);
    u32 read_control_register(const u32 reg);
    void write_control_register(const u32 reg, const u32 value);

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
    i16 sxy[3];         // SYX0, SXY1, SXY2 (and SXYP); 3 stages
    u16 sz[4];          // S30, SZ1, SZ2, SZ3; 4 stages
    u32 rgb[3];         // 3 stages
    i64 mac0;           // 32-bit maths accumulator (value)
    i64 mac1;           // 32-bit maths accumulator (vector)
    i64 mac2;           // 32-bit maths accumulator (vector)
    i64 mac3;           // 32-bit maths accumulator (vector)
    u16 irgb;           // For converting 48-bit vs 15-bit colour
    u16 orgb;           // For converting 48-bit vs 15-bit colour
    u32 lzcs;           // For counting leading zeroes and ones
    u32 lzcr;           // For counting leading zeroes and ones

    // Control registers
    i16 rt[3][3];       // Rotation matrix
    i32 tr[3];          // Translation vector
    i16 l[3][3];        // Light source matrix
    u32 rbk;            // Background colour (r, g, b)
    u32 gbk;            // Background colour (r, g, b)
    u32 bbk;            // Background colour (r, g, b)
    i16 lr[3][3];       // Light colour matrix
    u32 rfc;            // Far colour (r, g, b)
    u32 gfc;            // Far colour (r, g, b)
    u32 bfc;            // Far colour (r, g, b)
    u32 ofx;            // Screen offset (x, y)
    u32 ofy;            // Screen offset (x, y)
    u16 h;              // Projection plane distance
    i16 dqa;            // Depth queuing parameter A
    i32 dqb;            // Depth queuing parameter B
    i16 zsf3;           // Average Z scale factors
    i16 zsf4;           // Average Z scale factors
    u32 flag;           // Flag register

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