#include "bus.h"

#define WITHIN(base, size) (address >= base && address < base + size)

Bus::Bus(const std::string& bios_path)
{
    memset(bios, 0, sizeof(bios));
    memset(ram, 0, sizeof(ram));
    read_from_file(bios, bios_path);
}

u8 Bus::read_u8(const u32 address)
{
    // Main RAM
    if (address < 2048 * 1024)
        return ram[address];
    if (WITHIN(0x80000000, 2048 * 1024))
        return ram[address - 0x80000000];
    if (WITHIN(0xA0000000, 2048 * 1024))
        return ram[address - 0xA0000000];

    // Expansion 1
    if (WITHIN(0x1F000000, 8192 * 1024) || WITHIN(0x9F000000, 8192 * 1024) || WITHIN(0xBF000000, 8192 * 1024))
        return 0xff;

    // Expansion 2
    if (WITHIN(0x1F802000, 8 * 1024) || WITHIN(0x9F802000, 8 * 1024) || WITHIN(0xBF802000, 8 * 1024))
        return 0xff;

    // BIOS ROM
    if (WITHIN(0x1FC00000, 4096 * 1024))
        return bios[address - 0x1FC00000];
    if (WITHIN(0x9FC00000, 4096 * 1024))
        return bios[address - 0x9FC00000];
    if (WITHIN(0xBFC00000, 4096 * 1024))
        return bios[address - 0xBFC00000];

    dbg("TODO: unknown bus read", dbg::hex(address));
    exit(1);
}

u16 Bus::read_u16(const u32 address)
{
    // I/O
    if (WITHIN(0x1F801000, 4096) || address == 0xFFFE0130)
        return io_read_u16(address);

    return (u32)read_u8(address) | ((u32)read_u8(address + 1) << 8);
}

u32 Bus::read_u32(const u32 address)
{
    // I/O
    if (WITHIN(0x1F801000, 4096) || address == 0xFFFE0130)
        return io_read_u32(address);

    return (u32)read_u8(address) |
           ((u32)read_u8(address + 1) << 8) |
           ((u32)read_u8(address + 2) << 16) |
           ((u32)read_u8(address + 3) << 24);
}

void Bus::write_u8(const u32 address, const u8 value)
{
    // Main RAM
    if (address < 2048 * 1024)
        { ram[address] = value; return; }
    if (WITHIN(0x80000000, 2048 * 1024))
        { ram[address - 0x80000000] = value; return; }
    if (WITHIN(0xA0000000, 2048 * 1024))
        { ram[address - 0xA0000000] = value; return; }

    // Expansion 1
    if (WITHIN(0x1F000000, 8192 * 1024) || WITHIN(0x9F000000, 8192 * 1024) || WITHIN(0xBF000000, 8192 * 1024))
        return;

    // Expansion 2
    if (WITHIN(0x1F802000, 8 * 1024) || WITHIN(0x9F802000, 8 * 1024) || WITHIN(0xBF802000, 8 * 1024))
        return;

    // BIOS ROM
    if (WITHIN(0x1FC00000, 4096 * 1024) || WITHIN(0x9FC00000, 4096 * 1024) || WITHIN(0xBFC00000, 4096 * 1024))
        return;

    dbg("TODO: unknown bus write", dbg::hex(address), dbg::hex(value));
    exit(1);
}

void Bus::write_u16(const u32 address, const u16 value)
{
    // I/O
    if (WITHIN(0x1F801000, 4096))
    {
        io_write_u32(address, value);
        return;
    }

    write_u8(address + 0, (u8)(value >> 0));
    write_u8(address + 1, (u8)(value >> 8));
}

void Bus::write_u32(const u32 address, const u32 value)
{
    // I/O
    if (WITHIN(0x1F801000, 4096) || address == 0xFFFE0130)
    {
        io_write_u32(address, value);
        return;
    }

    write_u8(address + 0, (u8)(value >> 0));
    write_u8(address + 1, (u8)(value >> 8));
    write_u8(address + 2, (u8)(value >> 16));
    write_u8(address + 3, (u8)(value >> 24));
}

void Bus::read_from_file(u8* buffer, const std::string& filename)
{
    // Check isn't folder
    if (!std::filesystem::is_regular_file(filename))
        throw std::runtime_error(filename + " is not a file");

    // Open file
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("unable to open file " + filename);

    // Get file length
    file.seekg(0, std::ios::end);
    const auto file_length = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read file contents into buffer
    file.read(reinterpret_cast<char*>(buffer), file_length);
    file.close();
}

u16 Bus::io_read_u16(const u32 address)
{
    dbg("TODO: unrecognised I/O read", dbg::hex(address));
    exit(1);
}

u32 Bus::io_read_u32(const u32 address)
{
    // I_STAT
    if (address == 0x1F801070)
        return i_stat;

    // I_MASK
    if (address == 0x1F801074)
        return i_mask;

    // Timer values
    if (address == 0x1F801100) { dbg("TODO: accurate timer values"); return timer_0; }
    if (address == 0x1F801110) { dbg("TODO: accurate timer values"); return timer_1; }
    if (address == 0x1F801120) { dbg("TODO: accurate timer values"); return timer_2; }

    dbg("TODO: unrecognised I/O read", dbg::hex(address));
    exit(1);
}

void Bus::io_write_u16(const u32 address, const u16 value)
{
    // SPU control registers (some are 16-bits wide as opposed to 32 so skip here)
    if (address >= 0x1F801D80 && address <= 0x1F801DBC)
        return;

    dbg("TODO: unrecognised I/O write", dbg::hex(address), dbg::hex(value));
    exit(1);
}

void Bus::io_write_u32(const u32 address, const u32 value)
{
    // BIOS ROM delay / size
    if (address == 0x1f801010) return;

    // RAM_SIZE
    if (address == 0x1F801060) return;

    // For COM
    if (address == 0x1F801020) return;

    // Expansion 1 base address
    if (address == 0x1F801000) return;

    // Expansion 2 base address
    if (address == 0x1F801004) return;

    // Expansion 1 delay / size
    if (address == 0x1F801008) return;

    // Expansion 2 delay / size
    if (address == 0x1F80101C) return;

    // Expansion 3 delay / size
    if (address == 0x1F80100C) return;

    // SPU delay / size
    if (address == 0x1F801014) return;

    // CDROM delay / size
    if (address == 0x1F801018) return;

    // SPU control registers (some are 16-bits wide as opposed to 32 so skip here)
    if (address >= 0x1F801D80 && address <= 0x1F801DBC)
        return;

    // Cache configuration register
    if (address == 0xFFFE0130)
    {
        dbg("TODO: wrote to cache configuration register", dbg::hex(value));
        return;
    }

    // I_STAT - NOTE: writing is to "acknowledge"
    if (address == 0x1F801070)
    {
        i_stat = value;
        if (value != 0)
        {
            dbg("TODO: unsupported i_stat value", dbg::hex(value));
            exit(1);
        }
        return;
    }

    // I_MASK
    if (address == 0x1F801074)
    {
        i_mask = value;
        if (value != 0)
        {
            dbg("TODO: unsupported i_mask value", dbg::hex(value));
            exit(1);
        }
        return;
    }

    // Timer counter modes
    if (address == 0x1F801104)
    {
        if (value != 0)
        {
            dbg("TODO: unsupported timer 0 counter mode", dbg::hex(value));
            exit(1);
        }
        timer_0 = 0;
        return;
    }
    if (address == 0x1F801114)
    {
        if (value != 0)
        {
            dbg("TODO: unsupported timer 1 counter mode", dbg::hex(value));
            exit(1);
        }
        timer_1 = 0;
        return;
    }
    if (address == 0x1F801124)
    {
        if (value != 0)
        {
            dbg("TODO: unsupported timer 2 counter mode", dbg::hex(value));
            exit(1);
        }
        timer_2 = 0;
        return;
    }

    // Timer values
    if (address == 0x1F801100) { timer_0 = value; return; }
    if (address == 0x1F801110) { timer_1 = value; return; }
    if (address == 0x1F801120) { timer_2 = value; return; }

    // Timer targets
    if (address == 0x1F801108) { timer_0_target = value; return; }
    if (address == 0x1F801118) { timer_1_target = value; return; }
    if (address == 0x1F801128) { timer_2_target = value; return; }

    dbg("TODO: unrecognised I/O write", dbg::hex(address), dbg::hex(value));
    exit(1);
}