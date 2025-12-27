#pragma once

class CP0
{
public:
    u32 bpc; // Breakpoint program counter
    u32 bda; // Breakpoint data address
    u32 tar; // Target address
    u32 dcic; // Debug and cache invalidate control
    u32 bad_a; // Bad address
    u32 bdam; // Breakpoint data address mask
    u32 bpcm; // Breakpoint program counter mask
    u32 cause; // Cause of exception
    u32 epc; // Exception program counter

    struct StatusRegister
    {
        bool boot_exception_vectors = true;
        bool isolate_cache = false;
        u8 interrupt_mask = 0;
        bool kernel_mode = true;
        bool interrupts_enabled = false;
        bool kernel_mode_previous = true;
        bool interrupts_enabled_previous = false;
        bool kernel_mode_old = true;
        bool interrupts_enabled_old = false;
        bool cp0_enabled = true;
        bool cp1_enabled = true;
        bool cp2_enabled = true;
        bool cp3_enabled = true;

        u32 read() const
        {
            return ((u32)cp3_enabled << 31)
                 | ((u32)cp2_enabled << 30)
                 | ((u32)cp1_enabled << 29)
                 | ((u32)cp0_enabled << 28)
                 | ((u32)boot_exception_vectors << 22)
                 | ((u32)isolate_cache << 16)
                 | ((u32)interrupt_mask << 8)
                 | ((u32)!kernel_mode_old << 5)
                 | ((u32)interrupts_enabled_old << 4)
                 | ((u32)!kernel_mode_previous << 3)
                 | ((u32)interrupts_enabled_previous << 2)
                 | ((u32)!kernel_mode << 1)
                 | ((u32)interrupts_enabled);
        }

        void write(const u32 value)
        {
            cp3_enabled = ((value >> 31) & 0b1) != 0;
            cp2_enabled = ((value >> 30) & 0b1) != 0;
            cp1_enabled = ((value >> 28) & 0b1) != 0;
            cp0_enabled = ((value >> 29) & 0b1) != 0;
            boot_exception_vectors = ((value >> 22) & 0b1) != 0;
            isolate_cache = ((value >> 16) & 0b1) != 0;
            interrupt_mask = (u8)(value >> 8);
            kernel_mode_old = ((value >> 5) & 0b1) == 0;
            interrupts_enabled_old = ((value >> 4) & 0b1) != 0;;
            kernel_mode_previous = ((value >> 3) & 0b1) == 0;
            interrupts_enabled_previous = ((value >> 2) & 0b1) != 0;;
            kernel_mode = ((value >> 1) & 0b1) == 0;;
            interrupts_enabled = (value & 0b1) != 0;
        }

        void on_exception()
        {
            kernel_mode_old = kernel_mode_previous;
            interrupts_enabled_old = interrupts_enabled_previous;
            kernel_mode_previous = kernel_mode;
            interrupts_enabled_previous = interrupts_enabled;
            kernel_mode = true;
            interrupts_enabled = false;
        }
    } sr;

    void write_register(const u32 reg, const u32 value)
    {
        switch (reg)
        {
            // Read-only
            case 6:
            case 8:
            case 14:
            case 15:
            break;

            case 3: bpc = value; break;
            case 5: bda = value; break;
            case 7: dcic = value; break;
            case 9: bdam = value; break;
            case 11: bpcm = value; break;
            case 12: sr.write(value); break;

            case 13:
                // Only bits 8 and 9 are writable
                cause = cause & 0b11111111111111111111110011111111;
                cause = cause | (value & 0b11111111111111111111110011111111);
                break;

            default: dbg("TODO: unknown CP0 register", reg, value);
        }
    }

    u32 read_register(const u32 reg)
    {
        switch (reg)
        {
            case 3: return bpc;
            case 5: return bda;
            case 6: return tar;
            case 7: return dcic;
            case 8: return bad_a;
            case 9: return bdam;
            case 11: return bpcm;
            case 12: return sr.read();
            case 13: return cause;
            case 14: return epc;
            case 15: return 2; // Processor revision identifier
            default: dbg("TODO: unknown CP0 register", reg);
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
                sr.kernel_mode = sr.kernel_mode_previous;
                sr.interrupts_enabled = sr.interrupts_enabled_previous;
                sr.kernel_mode_previous = sr.kernel_mode_old;
                sr.interrupts_enabled_previous = sr.interrupts_enabled_old;
                break;
            }

            default:
                dbg("TODO: unsupported PSX CP0 operation", dbg::hex(opcode & 0x3f));
                exit(1);
        }
    }

    void set_interrupt_due(const bool due)
    {
        if (due) cause |= 1 << 10;
        else cause &= (u32)~(1 << 10);
    }

    bool interrupt_pending() const
    {
        const u32 cause_interrupt_bits = cause >> 8;
        return sr.interrupts_enabled && (sr.interrupt_mask & cause_interrupt_bits) != 0;
    }
};