static const char* reg_name(u32 reg)
{
    static const char* names[32] =
    {
        "$zero","$at","$v0","$v1","$a0","$a1","$a2","$a3",
        "$t0","$t1","$t2","$t3","$t4","$t5","$t6","$t7",
        "$s0","$s1","$s2","$s3","$s4","$s5","$s6","$s7",
        "$t8","$t9","$k0","$k1","$gp","$sp","$fp","$ra"
    };
    return names[reg];
}

void trace(u32 instruction)
{
    const u32 opcode = instruction >> 26;

    switch (opcode)
    {
        case 0x00:
        {
            const u32 funct = instruction & 0x3F;
            const u32 rs = (instruction >> 21) & 0x1F;
            const u32 rt = (instruction >> 16) & 0x1F;
            const u32 rd = (instruction >> 11) & 0x1F;
            const u32 shamt = (instruction >> 6) & 0x1F;

            switch (funct)
            {
                case 0x00: printf("sll %s, %s, %u\n", reg_name(rd), reg_name(rt), shamt); break;
                case 0x02: printf("srl %s, %s, %u\n", reg_name(rd), reg_name(rt), shamt); break;
                case 0x03: printf("sra %s, %s, %u\n", reg_name(rd), reg_name(rt), shamt); break;
                case 0x04: printf("sllv %s, %s, %s\n", reg_name(rd), reg_name(rt), reg_name(rs)); break;
                case 0x06: printf("srlv %s, %s, %s\n", reg_name(rd), reg_name(rt), reg_name(rs)); break;
                case 0x07: printf("srav %s, %s, %s\n", reg_name(rd), reg_name(rt), reg_name(rs)); break;
                case 0x08: printf("jr %s\n", reg_name(rs)); break;
                case 0x09: printf("jalr %s, %s\n", reg_name(rd), reg_name(rs)); break;
                case 0x0c: printf("syscall\n"); break;
                case 0x0d: printf("break\n"); break;
                case 0x10: printf("mfhi %s\n", reg_name(rd)); break;
                case 0x11: printf("mthi %s\n", reg_name(rs)); break;
                case 0x12: printf("mflo %s\n", reg_name(rd)); break;
                case 0x13: printf("mtlo %s\n", reg_name(rs)); break;
                case 0x18: printf("mult %s, %s\n", reg_name(rs), reg_name(rt)); break;
                case 0x19: printf("multu %s, %s\n", reg_name(rs), reg_name(rt)); break;
                case 0x1a: printf("div %s, %s\n", reg_name(rs), reg_name(rt)); break;
                case 0x1b: printf("divu %s, %s\n", reg_name(rs), reg_name(rt)); break;
                case 0x20: printf("add %s, %s, %s\n", reg_name(rd), reg_name(rs), reg_name(rt)); break;
                case 0x21: printf("addu %s, %s, %s\n", reg_name(rd), reg_name(rs), reg_name(rt)); break;
                case 0x22: printf("sub %s, %s, %s\n", reg_name(rd), reg_name(rs), reg_name(rt)); break;
                case 0x23: printf("subu %s, %s, %s\n", reg_name(rd), reg_name(rs), reg_name(rt)); break;
                case 0x24: printf("and %s, %s, %s\n", reg_name(rd), reg_name(rs), reg_name(rt)); break;
                case 0x25: printf("or %s, %s, %s\n", reg_name(rd), reg_name(rs), reg_name(rt)); break;
                case 0x26: printf("xor %s, %s, %s\n", reg_name(rd), reg_name(rs), reg_name(rt)); break;
                case 0x27: printf("nor %s, %s, %s\n", reg_name(rd), reg_name(rs), reg_name(rt)); break;
                case 0x2a: printf("slt %s, %s, %s\n", reg_name(rd), reg_name(rs), reg_name(rt)); break;
                case 0x2b: printf("sltu %s, %s, %s\n", reg_name(rd), reg_name(rs), reg_name(rt)); break;
                default: printf("UNKNOWN_R_TYPE 0x%x\n", funct); break;
            }
        }
        break;

        case 0x01:
        {
            const u32 rs = (instruction >> 21) & 0x1F;
            const u32 rt = (instruction >> 16) & 0x1F;
            const u32 imm = instruction & 0xFFFF;

            switch (rt)
            {
                case 0x00: printf("bltz %s, 0x%x\n", reg_name(rs), imm); break;
                case 0x01: printf("bgez %s, 0x%x\n", reg_name(rs), imm); break;
                case 0x10: printf("bltzal %s, 0x%x\n", reg_name(rs), imm); break;
                case 0x11: printf("bgezal %s, 0x%x\n", reg_name(rs), imm); break;
                default: printf("UNKNOWN_REGIMM rs=%s rt=0x%x imm=0x%x\n", reg_name(rs), rt, imm); break;
            }
        }
        break;

        case 0x02: printf("j 0x%x\n", instruction & 0x03FFFFFF); break;
        case 0x03: printf("jal 0x%x\n", instruction & 0x03FFFFFF); break;
        case 0x04: printf("beq %s, %s, 0x%x\n", reg_name((instruction >> 21) & 0x1F), reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF); break;
        case 0x05: printf("bne %s, %s, 0x%x\n", reg_name((instruction >> 21) & 0x1F), reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF); break;
        case 0x06: printf("blez %s, 0x%x\n", reg_name((instruction >> 21) & 0x1F), instruction & 0xFFFF); break;
        case 0x07: printf("bgtz %s, 0x%x\n", reg_name((instruction >> 21) & 0x1F), instruction & 0xFFFF); break;
        case 0x08: printf("addi %s, %s, 0x%x\n", reg_name((instruction >> 16) & 0x1F), reg_name((instruction >> 21) & 0x1F), instruction & 0xFFFF); break;
        case 0x09: printf("addiu %s, %s, 0x%x\n", reg_name((instruction >> 16) & 0x1F), reg_name((instruction >> 21) & 0x1F), instruction & 0xFFFF); break;
        case 0x0a: printf("slti %s, %s, 0x%x\n", reg_name((instruction >> 16) & 0x1F), reg_name((instruction >> 21) & 0x1F), instruction & 0xFFFF); break;
        case 0x0b: printf("sltiu %s, %s, 0x%x\n", reg_name((instruction >> 16) & 0x1F), reg_name((instruction >> 21) & 0x1F), instruction & 0xFFFF); break;
        case 0x0c: printf("andi %s, %s, 0x%x\n", reg_name((instruction >> 16) & 0x1F), reg_name((instruction >> 21) & 0x1F), instruction & 0xFFFF); break;
        case 0x0d: printf("ori %s, %s, 0x%x\n", reg_name((instruction >> 16) & 0x1F), reg_name((instruction >> 21) & 0x1F), instruction & 0xFFFF); break;
        case 0x0e: printf("xori %s, %s, 0x%x\n", reg_name((instruction >> 16) & 0x1F), reg_name((instruction >> 21) & 0x1F), instruction & 0xFFFF); break;
        case 0x0f: printf("lui %s, 0x%x\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF); break;
        case 0x10: printf("cop0 0x%x\n", instruction & 0x03FFFFFF); break;
        case 0x11: printf("cop1 0x%x\n", instruction & 0x03FFFFFF); break;
        case 0x12: printf("cop2 0x%x\n", instruction & 0x03FFFFFF); break;
        case 0x13: printf("cop3 0x%x\n", instruction & 0x03FFFFFF); break;
        case 0x20: printf("lb %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x21: printf("lh %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x22: printf("lwl %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x23: printf("lw %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x24: printf("lbu %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x25: printf("lhu %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x26: printf("lwr %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x28: printf("sb %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x29: printf("sh %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x2a: printf("swl %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x2b: printf("sw %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x2e: printf("swr %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x30: printf("lwc0 %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x31: printf("lwc1 %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x32: printf("lwc2 %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x33: printf("lwc3 %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x38: printf("swc0 %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x39: printf("swc1 %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x3a: printf("swc2 %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;
        case 0x3b: printf("swc3 %s, 0x%x(%s)\n", reg_name((instruction >> 16) & 0x1F), instruction & 0xFFFF, reg_name((instruction >> 21) & 0x1F)); break;

        default: printf("UNKNOWN_OPCODE 0x%x\n", opcode); break;
    }
}