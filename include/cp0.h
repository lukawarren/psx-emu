#pragma once

class CP0
{
public:
    u32 sr; // Status register

    void write_register(const u32 reg, const u32 value)
    {
        switch (reg)
        {
            case 3:
            case 5:
            case 6:
            case 7:
            case 9:
            case 11:
                dbg("TODO: CP0 breakpoint registers");
                if (value != 0) exit(1);
                break;

            case 12:
                sr = value;
                if (isolate_cache()) dbg("TODO: CP0 isolate cache bit set");
                else dbg("CP0 isolate cache bit unset");
                break;

            case 13:
                dbg("TODO: CP0 CAUSE register");
                if (value != 0) exit(1);
                break;

            default:
                dbg("TODO: unknown CP0 register", reg, value);
                if (value != 0) exit(1);
        }
    }

    u32 read_register(const u32 reg)
    {
        switch (reg)
        {
            case 12: return sr;
            default: dbg("TODO: unknown CP0 register", reg); exit(1);
        }

        return 0;
    }

    void execute(const u32 opcode)
    {
        switch (opcode & 0x3f)
        {
            case 0x10:
            {
                // RFE (restore from exception)
                const u32 mode = sr & 0x3f;
                sr &= ~0x3fU;
                sr |= mode >> 2;
                break;
            }

            default:
                dbg("TODO: unknown CP0 operation", dbg::hex(opcode & 0x3f));
                exit(1);
        }
    }

    inline bool isolate_cache() const
    {
        return sr & (1 << 16);
    }
};