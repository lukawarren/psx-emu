#pragma once
#include "bus.h"
#include "cp0.h"

class CPU
{
public:
    CPU();
    void execute();
    Bus bus;

    // Frequency in Hz
    constexpr static u32 frequency = 33868800;

    inline void configure_from_exe(const u32 r28, const u32 sp, const u32 pc)
    {
        registers[28] = r28;
        if (sp != 0)
        {
            registers[29] = sp;
            registers[30] = sp;
        }
        this->pc = pc;
        next_pc = pc + 4;
    }

private:
    /*
        Name       Alias    Common Usage
        R0         zero     Constant (always 0)
        R1         at       Assembler temporary (destroyed by some assembler pseudoinstructions!)
        R2-R3      v0-v1    Subroutine return values, may be changed by subroutines
        R4-R7      a0-a3    Subroutine arguments, may be changed by subroutines
        R8-R15     t0-t7    Temporaries, may be changed by subroutines
        R16-R23    s0-s7    Static variables, must be saved by subs
        R24-R25    t8-t9    Temporaries, may be changed by subroutines
        R26-R27    k0-k1    Reserved for kernel (destroyed by some IRQ handlers!)
        R28        gp       Global pointer (rarely used)
        R29        sp       Stack pointer
        R30        fp(s8)   Frame Pointer, or 9th Static variable, must be saved
        R31        ra       Return address (used so by JAL,BLTZAL,BGEZAL opcodes)
        -          pc       Program counter
        -          hi,lo    Multiply/divide results, may be changed by subroutines
    */
    u32 registers[32] = {};
    u32 pc;
    u32 hi;
    u32 lo;
    CP0 cp0;

    /*
        Note that due to "load delay", load instructions are async and so the
        relevant register will only be written to after the *next* instruction
        has finished.
    */
    std::pair<u8, u32> delayed_load = {};
    std::pair<u8, u32> delayed_load_next = {};

    // Branches are delayed by one - i.e. we run the next instruction, *then* branch
    u32 next_pc;
    u32 previous_pc;
    bool due_branch_delay = false;

    // Load delay
    void write_register(const u32 reg, const u32 value);
    void write_register_delayed(const u32 reg, const u32 value);
    u32 read_register_for_lwl_and_lwr(const u32 reg);

    // Instruction decoding
    inline u32 get_rd(const u32 instruction) { return (instruction >> 11) & 0b11111; }
    inline u32 get_rt(const u32 instruction) { return (instruction >> 16) & 0b11111; }
    inline u32 get_rs(const u32 instruction) { return (instruction >> 21) & 0b11111; }
    inline u32 get_immediate(const u32 instruction) { return instruction & 0xffff; }
    inline i32 get_signed_immediate(const u32 instruction) { return (i16)instruction; }
    inline u32 get_jump_address(const u32 instruction) { return (pc & 0xf0000000) | ((instruction & 0x03ffffff) << 2); }
    inline u32 get_offset(const u32 instruction) { return ((u32)((i16)instruction)) << 2; }
    inline u32 get_address(const u32 instruction) { return (u32)((i32)registers[get_rs(instruction)] + (i32)get_signed_immediate(instruction)); }
    inline u32 get_shamt(const u32 instruction) { return (instruction >> 6) & 0b11111; }
    inline u32 get_coprocessor(const u32 instruction) { return (instruction >> 26) & 0b11; }

    // Bus
    u8 read_u8(const u32 address);
    u16 read_u16(const u32 address);
    u32 read_u32(const u32 address);
    void write_u8(const u32 address, const u8 value);
    void write_u16(const u32 address, const u16 value);
    void write_u32(const u32 address, const u32 value);

    // Exceptions
    enum class Exception
    {
        AddressErrorLoad = 0x4,
        AddressErrorStore = 0x5,
        Syscall = 0x8,
        ArithmeticOverflow = 0xc
    };
    void raise_exception(const Exception exception);

    inline void branch(const u32 instruction, const bool condition, const bool link = false)
    {
        if (condition)
        {
            next_pc = pc + get_offset(instruction);
            due_branch_delay = true;
        }

        if (link)
            write_register(31, pc + 4);
    }

    void j      (const u32 instruction);
    void jal    (const u32 instruction);
    void beq    (const u32 instruction);
    void bne    (const u32 instruction);
    void blez   (const u32 instruction);
    void bgtz   (const u32 instruction);
    void bltzal (const u32 instruction);
    void bgezal (const u32 instruction);
    void bltz   (const u32 instruction);
    void bgez   (const u32 instruction);
    void addi   (const u32 instruction);
    void addiu  (const u32 instruction);
    void slti   (const u32 instruction);
    void sltiu  (const u32 instruction);
    void andi   (const u32 instruction);
    void ori    (const u32 instruction);
    void xori   (const u32 instruction);
    void lui    (const u32 instruction);
    void mfcz   (const u32 instruction);
    void cfcz   (const u32 instruction);
    void mtcz   (const u32 instruction);
    void ctcz   (const u32 instruction);
    void copz   (const u32 instruction);
    void lb     (const u32 instruction);
    void lh     (const u32 instruction);
    void lwl    (const u32 instruction);
    void lw     (const u32 instruction);
    void lbu    (const u32 instruction);
    void lhu    (const u32 instruction);
    void lwr    (const u32 instruction);
    void sb     (const u32 instruction);
    void sh     (const u32 instruction);
    void swl    (const u32 instruction);
    void sw     (const u32 instruction);
    void swr    (const u32 instruction);
    void lwc0   (const u32 instruction);
    void lwc1   (const u32 instruction);
    void lwc2   (const u32 instruction);
    void lwc3   (const u32 instruction);
    void swc0   (const u32 instruction);
    void swc1   (const u32 instruction);
    void swc2   (const u32 instruction);
    void swc3   (const u32 instruction);
    void sll    (const u32 instruction);
    void srl    (const u32 instruction);
    void sra    (const u32 instruction);
    void sllv   (const u32 instruction);
    void srlv   (const u32 instruction);
    void srav   (const u32 instruction);
    void jr     (const u32 instruction);
    void jalr   (const u32 instruction);
    void syscall(const u32 instruction);
    void _break (const u32 instruction);
    void mfhi   (const u32 instruction);
    void mthi   (const u32 instruction);
    void mflo   (const u32 instruction);
    void mtlo   (const u32 instruction);
    void mult   (const u32 instruction);
    void multu  (const u32 instruction);
    void div    (const u32 instruction);
    void divu   (const u32 instruction);
    void add    (const u32 instruction);
    void addu   (const u32 instruction);
    void sub    (const u32 instruction);
    void subu   (const u32 instruction);
    void _and   (const u32 instruction);
    void _or    (const u32 instruction);
    void _xor   (const u32 instruction);
    void nor    (const u32 instruction);
    void slt    (const u32 instruction);
    void sltu   (const u32 instruction);
};
