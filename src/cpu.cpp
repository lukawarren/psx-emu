#include "cpu.h"
#include "trace.h"

#define ENABLE_TRACE 0
#define ENABLE_SERIAL 1
#define SIDELOAD_EXE 1

CPU::CPU() : bus(*this, "../SCPH1001.bin")
{
    pc = 0xbfc00000;
    next_pc = pc + 4;
    registers[0] = 0;
}

void CPU::execute()
{
    delayed_load_next.first = 0;

    const auto process_load_delay = [&]()
    {
        if (delayed_load.first != 0)
        {
            registers[delayed_load.first] = delayed_load.second;
            delayed_load.first = 0;
        }

        if (delayed_load_next.first != 0)
        {
            delayed_load = delayed_load_next;
            delayed_load_next = {};
        }
    };

    // Check for misaligned access
    if ((pc & 0b11) != 0)
    {
        previous_pc = pc;
        cp0.bad_a = pc;
        raise_exception(Exception::AddressErrorLoad);
        process_load_delay();
        return;
    }

    // Fetch opcode
    // TODO: timing, I-cache, etc.
    const u32 instruction = bus.read_u32(pc);
    previous_pc = pc;

#if ENABLE_TRACE
    trace(instruction);
#endif

#if ENABLE_SERIAL
    if (((pc & 0x1FFFFFFF) == 0xA0 && registers[9] == 0x3C) || ((pc & 0x1FFFFFFF) == 0xB0 && registers[9] == 0x3D))
        printf("%c", (u8)registers[4]);
#endif

#if SIDELOAD_EXE
    static bool did_sideload = false;
    if (pc == 0x80030000 && !did_sideload)
    {
        did_sideload = true;
        // bus.sideload_exe("../tests/gpu/triangle/triangle.exe");
        bus.sideload_exe("../tests/psxtest_cpu.exe");
    }
#endif

    // Get next program counter
    pc = next_pc;
    next_pc = pc + 4;
    due_branch_delay = false;

    // Decode
    switch (instruction >> 26)
    {
        case 0x00:
            switch (instruction & 0b111111)
            {
                case 0x00: sll(instruction);     break;
                case 0x02: srl(instruction);     break;
                case 0x03: sra(instruction);     break;
                case 0x04: sllv(instruction);    break;
                case 0x06: srlv(instruction);    break;
                case 0x07: srav(instruction);    break;
                case 0x08: jr(instruction);      break;
                case 0x09: jalr(instruction);    break;
                case 0x0c: syscall(instruction); break;
                case 0x0d: _break(instruction);  break;
                case 0x10: mfhi(instruction);    break;
                case 0x11: mthi(instruction);    break;
                case 0x12: mflo(instruction);    break;
                case 0x13: mtlo(instruction);    break;
                case 0x18: mult(instruction);    break;
                case 0x19: multu(instruction);   break;
                case 0x1a: div(instruction);     break;
                case 0x1b: divu(instruction);    break;
                case 0x20: add(instruction);     break;
                case 0x21: addu(instruction);    break;
                case 0x22: sub(instruction);     break;
                case 0x23: subu(instruction);    break;
                case 0x24: _and(instruction);    break;
                case 0x25: _or(instruction);     break;
                case 0x26: _xor(instruction);    break;
                case 0x27: nor(instruction);     break;
                case 0x2a: slt(instruction);     break;
                case 0x2b: sltu(instruction);    break;
                default:
                    dbg("TODO: reserved instruction exception (1)", dbg::hex(instruction & 0b111111));
                    exit(1);
                    break;
            }
        break;

        case 0x01:
            switch ((instruction >> 16) & 0b11111)
            {
                case 0x10: bltzal(instruction); break;
                case 0x11: bgezal(instruction); break;
                default:
                    if ((instruction & (1 << 16)) == 0)
                        bltz(instruction);
                    else
                        bgez(instruction);
                break;
            }
        break;

        case 0x02: j(instruction);      break;
        case 0x03: jal(instruction);    break;
        case 0x04: beq(instruction);    break;
        case 0x05: bne(instruction);    break;
        case 0x06: blez(instruction);   break;
        case 0x07: bgtz(instruction);   break;
        case 0x08: addi(instruction);   break;
        case 0x09: addiu(instruction);  break;
        case 0x0a: slti(instruction);   break;
        case 0x0b: sltiu(instruction);  break;
        case 0x0c: andi(instruction);   break;
        case 0x0d: ori(instruction);    break;
        case 0x0e: xori(instruction);   break;
        case 0x0f: lui(instruction);    break;

        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        {
            const u32 bits = (instruction >> 21) & 0x1f;
            if (bits == 0x00) mfcz(instruction);
            else if (bits == 0x02) cfcz(instruction);
            else if (bits == 0x04) mtcz(instruction);
            else if (bits == 0x06) ctcz(instruction);
            else if (bits >= 0x10 && bits <= 0x1f) copz(instruction);
            else
            {
                dbg("TODO: reserved instruction exception (3)", dbg::hex(bits));
                exit(1);
                break;
            }
        }
        break;

        case 0x20: lb(instruction);     break;
        case 0x21: lh(instruction);     break;
        case 0x22: lwl(instruction);    break;
        case 0x23: lw(instruction);     break;
        case 0x24: lbu(instruction);    break;
        case 0x25: lhu(instruction);    break;
        case 0x26: lwr(instruction);    break;
        case 0x28: sb(instruction);     break;
        case 0x29: sh(instruction);     break;
        case 0x2a: swl(instruction);    break;
        case 0x2b: sw(instruction);     break;
        case 0x2e: swr(instruction);    break;
        case 0x30: lwc0(instruction);   break;
        case 0x31: lwc1(instruction);   break;
        case 0x32: lwc2(instruction);   break;
        case 0x33: lwc3(instruction);   break;
        case 0x38: swc0(instruction);   break;
        case 0x39: swc1(instruction);   break;
        case 0x3a: swc2(instruction);   break;
        case 0x3b: swc3(instruction);   break;
        default:
            dbg("TODO: reserved instruction exception (2)", dbg::hex(instruction >> 26));
            exit(1);
            break;
    }

    process_load_delay();

    bus.clock();
}

void CPU::write_register(const u32 reg, const u32 value)
{
    if (reg == 0) return;
    registers[reg] = value;

    // Overwrite any delayed loads
    if (delayed_load.first == reg)
        delayed_load.first = 0;
}

void CPU::write_register_delayed(const u32 reg, const u32 value)
{
    if (reg == 0) return;
    delayed_load_next = { reg, value };

    // Cancel the one that came before
    if (delayed_load.first == reg)
        delayed_load.first = 0;
}

u32 CPU::read_register_for_lwl_and_lwr(const u32 reg)
{
    return delayed_load.first == reg ? delayed_load.second : registers[reg];
}

u8 CPU::read_u8(const u32 address)
{
    return bus.read_u8(address);
}

u16 CPU::read_u16(const u32 address)
{
    return bus.read_u16(address);
}

u32 CPU::read_u32(const u32 address)
{
    return bus.read_u32(address);
}

void CPU::write_u8(const u32 address, const u8 value)
{
    // TODO
    if (cp0.sr.isolate_cache) return;
    bus.write_u8(address, value);
}

void CPU::write_u16(const u32 address, const u16 value)
{
    // TODO
    if (cp0.sr.isolate_cache) return;
    bus.write_u16(address, value);
}

void CPU::write_u32(const u32 address, const u32 value)
{
    // TODO
    if (cp0.sr.isolate_cache) return;
    bus.write_u32(address, value);
}

void CPU::raise_exception(const Exception exception)
{
    u32 handler = cp0.sr.boot_exception_vectors ? 0xbfc00100 : 0x80000000;
    if (exception == Exception::Breakpoint) handler += 0x40;
    else handler += 0x80;

    // Update CAUSE register with exception code
    cp0.cause = (u32)exception << 2;

    // Point EPC to the PC
    cp0.epc = previous_pc - (due_branch_delay ? 4 : 0);

    // Set BD bit if were in a branch delay, and update target address
    if (due_branch_delay)
    {
        cp0.cause |= (1U << 31);
        cp0.tar = next_pc;
    }
    else
        cp0.cause &= ~(1U << 31);

    // No branch delay; jump straight there
    pc = handler;
    next_pc = pc + 4;
    due_branch_delay = false;
}

void CPU::j(const u32 instruction)
{
    next_pc = get_jump_address(instruction);
    due_branch_delay = true;
}

void CPU::jal(const u32 instruction)
{
    next_pc = get_jump_address(instruction);
    due_branch_delay = true;
    write_register(31, pc + 4);
}

void CPU::beq(const u32 instruction)
{
    branch(instruction, registers[get_rs(instruction)] == registers[get_rt(instruction)]);
}

void CPU::bne(const u32 instruction)
{
    branch(instruction, registers[get_rs(instruction)] != registers[get_rt(instruction)]);
}

void CPU::blez(const u32 instruction)
{
    branch(instruction, (i32)registers[get_rs(instruction)] <= 0);
}

void CPU::bgtz(const u32 instruction)
{
    branch(instruction, (i32)registers[get_rs(instruction)] > 0);
}

void CPU::bltzal(const u32 instruction)
{
    branch(instruction, (i32)registers[get_rs(instruction)] < 0, true);
}

void CPU::bgezal(const u32 instruction)
{
    branch(instruction, (i32)registers[get_rs(instruction)] >= 0, true);
}

void CPU::bltz(const u32 instruction)
{
    branch(instruction, (i32)registers[get_rs(instruction)] < 0);
}

void CPU::bgez(const u32 instruction)
{
    branch(instruction, (i32)registers[get_rs(instruction)] >= 0);
}

void CPU::addi(const u32 instruction)
{
    i32 rs = (i32)registers[get_rs(instruction)];
    i32 imm = get_signed_immediate(instruction);

    if ((rs > 0 && imm > std::numeric_limits<i32>::max() - rs) ||
        (rs < 0 && imm < std::numeric_limits<i32>::min() - rs))
    {
        raise_exception(Exception::ArithmeticOverflow);
        return;
    }

    write_register(get_rt(instruction), (u32)(rs + imm));
}

void CPU::addiu(const u32 instruction)
{
    write_register(get_rt(instruction), registers[get_rs(instruction)] + (u32)get_signed_immediate(instruction));
}

void CPU::slti(const u32 instruction)
{
    write_register(get_rt(instruction), (i32)registers[get_rs(instruction)] < get_signed_immediate(instruction) ? 1 : 0);
}

void CPU::sltiu(const u32 instruction)
{
    write_register(get_rt(instruction), registers[get_rs(instruction)] < (u32)get_signed_immediate(instruction) ? 1 : 0);
}

void CPU::andi(const u32 instruction)
{
    write_register(get_rt(instruction), registers[get_rs(instruction)] & get_immediate(instruction));
}

void CPU::ori(const u32 instruction)
{
    write_register(get_rt(instruction), registers[get_rs(instruction)] | get_immediate(instruction));
}

void CPU::xori(const u32 instruction)
{
    write_register(get_rt(instruction), registers[get_rs(instruction)] ^ get_immediate(instruction));
}

void CPU::lui(const u32 instruction)
{
    write_register(get_rt(instruction), instruction << 16);
}

void CPU::mfcz(const u32 instruction)
{
    const u32 reg = get_rd(instruction);
    u32 value;

    switch (get_coprocessor(instruction))
    {
        case 0: value = cp0.read_register(reg); break;
        default: dbg("TODO: unknown co-processor"); exit(1); break;
    }

    write_register_delayed(get_rt(instruction), value);
}

void CPU::cfcz(const u32 instruction)
{
    dbg("todo");
    exit(1);
    (void)instruction;
}

void CPU::mtcz(const u32 instruction)
{
    const u32 reg = get_rd(instruction);
    const u32 value = registers[get_rt(instruction)];

    switch (get_coprocessor(instruction))
    {
        case 0: cp0.write_register(reg, value); break;
        default: dbg("TODO: unknown co-processor"); exit(1); break;
    }
}

void CPU::ctcz(const u32 instruction)
{
    dbg("todo");
    exit(1);
    (void)instruction;
}

void CPU::copz(const u32 instruction)
{
    switch (get_coprocessor(instruction))
    {
        case 0: cp0.execute(instruction & 0xffffff); break;
        default: dbg("TODO: unknown co-processor"); exit(1); break;
    }
}

void CPU::lb(const u32 instruction)
{
    write_register_delayed(get_rt(instruction), (u32)(i32)(i8)read_u8(get_address(instruction)));
}

void CPU::lh(const u32 instruction)
{
    const u32 address = get_address(instruction);
    if ((address & 1) != 0)
    {
        cp0.bad_a = address;
        raise_exception(Exception::AddressErrorLoad);
        return;
    }

    write_register_delayed(get_rt(instruction), (u32)(i32)(i16)read_u16(address));
}

void CPU::lwl(const u32 instruction)
{
    const u32 address = get_address(instruction);
    const u32 rt = get_rt(instruction);
    const u32 existing = read_register_for_lwl_and_lwr(rt);
    const u32 from_mem = read_u32(address & (~0b11U));
    const u32 shift = 8 * ((address & 0b11) ^ 0b11);
    const u32 mask = 0xffffffff << shift;
    write_register_delayed(rt, (existing & (~mask)) | (from_mem << shift));
}

void CPU::lw(const u32 instruction)
{
    const u32 address = get_address(instruction);
    if ((address & 0b11) != 0)
    {
        cp0.bad_a = address;
        raise_exception(Exception::AddressErrorLoad);
        return;
    }

    write_register_delayed(get_rt(instruction), read_u32(address));
}

void CPU::lbu(const u32 instruction)
{
    write_register_delayed(get_rt(instruction), read_u8(get_address(instruction)));
}

void CPU::lhu(const u32 instruction)
{
    const u32 address = get_address(instruction);
    if ((address & 1) != 0)
    {
        cp0.bad_a = address;
        raise_exception(Exception::AddressErrorLoad);
        return;
    }

    write_register_delayed(get_rt(instruction), read_u16(address));
}

void CPU::lwr(const u32 instruction)
{
    const u32 address = get_address(instruction);
    const u32 rt = get_rt(instruction);
    const u32 existing = read_register_for_lwl_and_lwr(rt);
    const u32 from_mem = read_u32(address & (~0b11U));
    const u32 shift = 8 * (address & 0b11);
    const u32 mask = 0xffffffff >> shift;
    write_register_delayed(rt, (existing & (~mask)) | (from_mem >> shift));
}

void CPU::sb(const u32 instruction)
{
    write_u8(get_address(instruction), (u8)registers[get_rt(instruction)]);
}

void CPU::sh(const u32 instruction)
{
    const u32 address = get_address(instruction);
    if ((address & 1) != 0)
    {
        cp0.bad_a = address;
        raise_exception(Exception::AddressErrorStore);
        return;
    }

    write_u16(address, (u16)registers[get_rt(instruction)]);
}

void CPU::swl(const u32 instruction)
{
    const u32 address = get_address(instruction);
    const u32 existing = read_u32(address & (~0b11U));
    const u32 reg = registers[get_rt(instruction)];
    const u32 shift = 8 * ((address & 0b11) ^ 0b11);
    const u32 mask = 0xffffffff >> shift;
    write_u32(address & (~0b11U), (existing & (~mask)) | (reg >> shift));
}

void CPU::sw(const u32 instruction)
{
    const u32 address = get_address(instruction);
    if ((address & 0b11) != 0)
    {
        cp0.bad_a = address;
        raise_exception(Exception::AddressErrorStore);
        return;
    }

    write_u32(address, registers[get_rt(instruction)]);
}

void CPU::swr(const u32 instruction)
{
    const u32 address = get_address(instruction);
    const u32 existing = read_u32(address & (~0b11U));
    const u32 reg = registers[get_rt(instruction)];
    const u32 shift = 8 * (address & 0b11);
    const u32 mask = 0xffffffff << shift;
    write_u32(address & (~0b11U), (existing & (~mask)) | (reg << shift));
}

void CPU::lwc0(const u32 instruction)
{
    dbg("todo");
    exit(1);(void)instruction;
}

void CPU::lwc1(const u32 instruction)
{
    dbg("todo");
    exit(1);(void)instruction;
}

void CPU::lwc2(const u32 instruction)
{
    dbg("todo");
    exit(1);(void)instruction;
}

void CPU::lwc3(const u32 instruction)
{
    dbg("todo");
    exit(1);(void)instruction;
}

void CPU::swc0(const u32 instruction)
{
    dbg("todo");
    exit(1);(void)instruction;
}

void CPU::swc1(const u32 instruction)
{
    dbg("todo");
    exit(1);(void)instruction;
}

void CPU::swc2(const u32 instruction)
{
    dbg("todo");
    exit(1);(void)instruction;
}

void CPU::swc3(const u32 instruction)
{
    dbg("todo");
    exit(1);(void)instruction;
}

void CPU::sll(const u32 instruction)
{
    write_register(get_rd(instruction), registers[get_rt(instruction)] << get_shamt(instruction));
}

void CPU::srl(const u32 instruction)
{
    write_register(get_rd(instruction), registers[get_rt(instruction)] >> get_shamt(instruction));
}

void CPU::sra(const u32 instruction)
{
    write_register(get_rd(instruction), (u32)((i32)registers[get_rt(instruction)] >> get_shamt(instruction)));
}

void CPU::sllv(const u32 instruction)
{
    const u32 shift_amount = registers[get_rs(instruction)] & 0b11111;
    write_register(get_rd(instruction), registers[get_rt(instruction)] << shift_amount);
}

void CPU::srlv(const u32 instruction)
{
    const u32 shift_amount = registers[get_rs(instruction)] & 0b11111;
    write_register(get_rd(instruction), registers[get_rt(instruction)] >> shift_amount);
}

void CPU::srav(const u32 instruction)
{
    const u32 shift_amount = registers[get_rs(instruction)] & 0b11111;
    write_register(get_rd(instruction), (u32)((i32)registers[get_rt(instruction)] >> shift_amount));
}

void CPU::jr(const u32 instruction)
{
    next_pc = registers[get_rs(instruction)];
    due_branch_delay = true;
}

void CPU::jalr(const u32 instruction)
{
    next_pc = registers[get_rs(instruction)];
    due_branch_delay = true;
    write_register(get_rd(instruction), pc + 4);
}

void CPU::syscall(const u32 instruction)
{
    (void)instruction;
    raise_exception(Exception::Syscall);
}

void CPU::_break(const u32 instruction)
{
    (void)instruction;
    cp0.dcic |= 1;
    cp0.bpc = previous_pc;
    raise_exception(Exception::Breakpoint);
}

void CPU::mfhi(const u32 instruction)
{
    write_register(get_rd(instruction), hi);
}

void CPU::mthi(const u32 instruction)
{
    hi = registers[get_rs(instruction)];
}

void CPU::mflo(const u32 instruction)
{
    write_register(get_rd(instruction), lo);
}

void CPU::mtlo(const u32 instruction)
{
    lo = registers[get_rs(instruction)];
}

void CPU::mult(const u32 instruction)
{
    const i64 a = (i64)(i32)registers[get_rs(instruction)];
    const i64 b = (i64)(i32)registers[get_rt(instruction)];
    const u64 ab = (u64)(a * b);
    lo = (u32)ab;
    hi = (u32)(ab >> 32);
}

void CPU::multu(const u32 instruction)
{
    const u64 a = registers[get_rs(instruction)];
    const u64 b = registers[get_rt(instruction)];
    const u64 ab = (u64)(a * b);
    lo = (u32)ab;
    hi = (u32)(ab >> 32);
}

void CPU::div(const u32 instruction)
{
    const i32 dividend = (i32)registers[get_rs(instruction)];
    const i32 divisor = (i32)registers[get_rt(instruction)];

    if (divisor == 0)
    {
        lo = dividend < 0 ? 1 : 0xffffffff;
        hi = (u32)dividend;
    }
    else if (divisor == -1 && (u32)dividend == -0x80000000)
    {
        lo = -0x80000000;
        hi = 0;
    }
    else
    {
        lo = (u32)(dividend / divisor);
        hi = (u32)(dividend % divisor);
    }
}

void CPU::divu(const u32 instruction)
{
    const u32 dividend = registers[get_rs(instruction)];
    const u32 divisor = registers[get_rt(instruction)];

    if (divisor == 0)
    {
        lo = 0xffffffff;
        hi = dividend;
    }
    else
    {
        lo = dividend / divisor;
        hi = dividend % divisor;
    }
}

void CPU::add(const u32 instruction)
{
    const i32 rs = (i32)registers[get_rs(instruction)];
    const i32 rt = (i32)registers[get_rt(instruction)];
    const i32 res = rs + rt;

    if (((rs ^ res) & (rt ^ res)) < 0)
    {
        raise_exception(Exception::ArithmeticOverflow);
        return;
    }

    write_register(get_rd(instruction), (u32)res);
}

void CPU::addu(const u32 instruction)
{
    write_register(get_rd(instruction), registers[get_rs(instruction)] + registers[get_rt(instruction)]);
}

void CPU::sub(const u32 instruction)
{
    const i32 rs = (i32)registers[get_rs(instruction)];
    const i32 rt = (i32)registers[get_rt(instruction)];
    const i32 res = rs - rt;

    if (((rs ^ rt) & (rs ^ res)) < 0)
    {
        raise_exception(Exception::ArithmeticOverflow);
        return;
    }

    write_register(get_rd(instruction), (u32)res);
}

void CPU::subu(const u32 instruction)
{
    write_register(get_rd(instruction), registers[get_rs(instruction)] - registers[get_rt(instruction)]);
}

void CPU::_and(const u32 instruction)
{
    write_register(get_rd(instruction), registers[get_rs(instruction)] & registers[get_rt(instruction)]);
}

void CPU::_or(const u32 instruction)
{
    write_register(get_rd(instruction), registers[get_rs(instruction)] | registers[get_rt(instruction)]);
}

void CPU::_xor(const u32 instruction)
{
    write_register(get_rd(instruction), registers[get_rs(instruction)] ^ registers[get_rt(instruction)]);
}

void CPU::nor(const u32 instruction)
{
    write_register(get_rd(instruction), ~(registers[get_rs(instruction)] | registers[get_rt(instruction)]));
}

void CPU::slt(const u32 instruction)
{
    const i32 rs = (i32)registers[get_rs(instruction)];
    const i32 rt = (i32)registers[get_rt(instruction)];
    write_register(get_rd(instruction), rs < rt ? 1 : 0);
}

void CPU::sltu(const u32 instruction)
{
    const u32 rs = registers[get_rs(instruction)];
    const u32 rt = registers[get_rt(instruction)];
    write_register(get_rd(instruction), rs < rt ? 1 : 0);
}
