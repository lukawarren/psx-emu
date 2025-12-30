#include "gte.h"

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

u32 GTE::read_data_register(const u32 reg)
{
    switch (reg)
    {
        case  0: return (u32)v0[0] | ((u32)v0[1] << 16);
        case  1: return (u32)(i32)v0[2];
        case  2: return (u32)v1[0] | ((u32)v1[1] << 16);
        case  3: return (u32)(i32)v1[2];
        case  4: return (u32)v2[0] | ((u32)v2[1] << 16);
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

        case 16: return (u32)sz[0];
        case 17: return (u32)sz[1];
        case 18: return (u32)sz[2];
        case 19: return (u32)sz[3];

        default:
            dbg("TODO: unknown GTE data register", reg);
            exit(1);
    }

}

void GTE::write_data_register(const u32 reg, const u32 value)
{
    switch (reg)
    {
        case  0: v0[0] = (i16)(value & 0xFFFF); v0[1] = (i16)(value >> 16); break;
        case  1: v0[2] = (i16)(value & 0xFFFF);                             break;
        case  2: v1[0] = (i16)(value & 0xFFFF); v1[1] = (i16)(value >> 16); break;
        case  3: v1[2] = (i16)(value & 0xFFFF);                             break;
        case  4: v2[0] = (i16)(value & 0xFFFF); v2[1] = (i16)(value >> 16); break;
        case  5: v2[2] = (i16)(value & 0xFFFF);                             break;

        case 24: mac0 = (i64)(i32)value; break;
        case 25: mac1 = (i64)(i32)value; break;
        case 26: mac2 = (i64)(i32)value; break;
        case 27: mac3 = (i64)(i32)value; break;

        default:
            dbg("TODO: unknown GTE data register", reg, dbg::hex(value));
            exit(1);
    }
}

u32 GTE::read_control_register(const u32 reg)
{
    switch (reg | 32)
    {
        // Sign extend if 16-bit; note that H is a hardware bug
        case 36: return (u32)(i32)(rt[2][2]);
        case 44: return (u32)(i32)(l[2][2]);
        case 52: return (u32)(i32)(lr[2][2]);
        case 58: return (u32)(i32)((i16)h);
        case 59: return (u32)(i32)(dqa);
        case 61: return (u32)(i32)(zsf3);
        case 62: return (u32)(i32)(zsf4);

        // Matrices
        case 32: return ((u32)(u16)rt[0][1] << 16) | (u32)rt[0][0];
        case 33: return ((u32)(u16)rt[1][0] << 16) | (u32)rt[0][2];
        case 34: return ((u32)(u16)rt[1][2] << 16) | (u32)rt[1][1];
        case 35: return ((u32)(u16)rt[2][1] << 16) | (u32)rt[2][0];
        case 40: return ((u32)(u16)l [0][1] << 16) | (u32)l [0][0];
        case 41: return ((u32)(u16)l [1][0] << 16) | (u32)l [0][2];
        case 42: return ((u32)(u16)l [1][2] << 16) | (u32)l [1][1];
        case 43: return ((u32)(u16)l [2][1] << 16) | (u32)l [2][0];
        case 48: return ((u32)(u16)lr[0][1] << 16) | (u32)lr[0][0];
        case 49: return ((u32)(u16)lr[1][0] << 16) | (u32)lr[0][2];
        case 50: return ((u32)(u16)lr[1][2] << 16) | (u32)lr[1][1];
        case 51: return ((u32)(u16)lr[2][1] << 16) | (u32)lr[2][0];

        // Normal
        case 37: return (u32)tr[0];
        case 38: return (u32)tr[1];
        case 39: return (u32)tr[2];
        case 45: return rbk;
        case 46: return gbk;
        case 47: return bbk;
        case 53: return rfc;
        case 54: return gfc;
        case 55: return bfc;
        case 56: return ofx;
        case 57: return ofy;
        case 60: return (u32)dqb;

        default:
            dbg("TODO: unknown GTE control register", reg);
            exit(1);
    }
}

void GTE::write_control_register(const u32 reg, const u32 value)
{
    switch (reg | 32)
    {
        // Matrices
        case 32: rt[0][0] = (i16)(u16)value; rt[0][1] = (i16)(u16)(value >> 16); break;
        case 33: rt[0][2] = (i16)(u16)value; rt[1][0] = (i16)(u16)(value >> 16); break;
        case 34: rt[1][1] = (i16)(u16)value; rt[1][2] = (i16)(u16)(value >> 16); break;
        case 35: rt[2][0] = (i16)(u16)value; rt[2][1] = (i16)(u16)(value >> 16); break;
        case 36: rt[2][2] = (i16)(u16)value;                                     break;
        case 40: l [0][0] = (i16)(u16)value; l [0][1] = (i16)(u16)(value >> 16); break;
        case 41: l [0][2] = (i16)(u16)value; l [1][0] = (i16)(u16)(value >> 16); break;
        case 42: l [1][1] = (i16)(u16)value; l [1][2] = (i16)(u16)(value >> 16); break;
        case 43: l [2][0] = (i16)(u16)value; l [2][1] = (i16)(u16)(value >> 16); break;
        case 44: l [2][2] = (i16)(u16)value;                                     break;
        case 48: lr[0][0] = (i16)(u16)value; lr[0][1] = (i16)(u16)(value >> 16); break;
        case 49: lr[0][2] = (i16)(u16)value; lr[1][0] = (i16)(u16)(value >> 16); break;
        case 50: lr[1][1] = (i16)(u16)value; lr[1][2] = (i16)(u16)(value >> 16); break;
        case 51: lr[2][0] = (i16)(u16)value; lr[2][1] = (i16)(u16)(value >> 16); break;
        case 52: lr[2][2] = (i16)(u16)value;                                     break;

        // Normal
        case 37: tr[0] = (i32)value;    break;
        case 38: tr[1] = (i32)value;    break;
        case 39: tr[2] = (i32)value;    break;
        case 45: rbk = value;           break;
        case 46: gbk = value;           break;
        case 47: bbk = value;           break;
        case 53: rfc = value;           break;
        case 54: gfc = value;           break;
        case 55: bfc = value;           break;
        case 56: ofx = value;           break;
        case 57: ofy = value;           break;
        case 58: h = (u16)value;        break;
        case 59: dqa = (i16)value;      break;
        case 60: dqb = (i32)value;      break;
        case 61: zsf3 = (i16)value;     break;
        case 62: zsf4 = (i16)value;     break;

        case 63:
            // Only certain bits writable; bit 31 special too
            flag = value & 0x7FFFF000;
            flag |= (u32)((value & 0x7F87E000) != 0) << 31;
            break;

        default:
            dbg("TODO: unknown GTE control register", reg, reg | 32, dbg::hex(value));
            exit(1);
    }
}

CycleCount GTE::rtps(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::nclip(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::op(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::dpcs(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::intpl(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
CycleCount GTE::mvmva(const u32 instruction) { dbg("TODO", dbg::hex(instruction & 0b111111)); exit(1); return 0; }
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
