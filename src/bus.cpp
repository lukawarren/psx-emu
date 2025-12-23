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
        { dbg("TODO: expansion 1"); return 0; }

    // Expansion 2
    if (WITHIN(0x1F802000, 8 * 1024) || WITHIN(0x9F802000, 8 * 1024) || WITHIN(0xBF802000, 8 * 1024))
        { dbg("TODO: expansion 2"); return 0; }

    // I/O ports
    if (WITHIN(0x1F801000, 4096))
        { dbg("TODO: IO ports"); return 0; }
    if (WITHIN(0x9F801000, 4096))
        { dbg("TODO: IO ports"); return 0; }
    if (WITHIN(0xBF801000, 4096))
        { dbg("TODO: IO ports"); return 0; }

    // BIOS ROM
    if (WITHIN(0x1FC00000, 4096 * 1024))
        return bios[address - 0x1FC00000];
    if (WITHIN(0x9FC00000, 4096 * 1024))
        return bios[address - 0x9FC00000];
    if (WITHIN(0xBFC00000, 4096 * 1024))
        return bios[address - 0xBFC00000];

    // MEMCONTROL
    if ((address >= 0x1f801000 && address <= 0x1f801000 + 36) || (address >= 0x1f801060 && address <= 0x1f801060 + 4))
    {
        dbg("TODO: memcontrol registers");
        return 0;
    }

    // CACHECONTROL
    if (WITHIN(0xFFFE0000, 512))
    {
        dbg("TODO: cache control registers");
        return 0;
    }

    printf("TODO: bus read with address 0x%X\n", address);
    exit(1);
}

u16 Bus::read_u16(const u32 address)
{
    return (u32)read_u8(address) | ((u32)read_u8(address + 1) << 8);
}

u32 Bus::read_u32(const u32 address)
{
    if (address == 0x1F000000)
    {
        dbg("TODO: expansion 1 base address");
        return 0x1f000000;
    }

    if (address == 0x1F000000)
    {
        dbg("TODO: expansion 2 base address");
        return 0x1f802000;
    }

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
        { dbg("TODO: expansion 1"); if (value != 0) exit(1); return; }

    // Expansion 2
    if (WITHIN(0x1F802000, 8 * 1024) || WITHIN(0x9F802000, 8 * 1024) || WITHIN(0xBF802000, 8 * 1024))
        { dbg("TODO: expansion 2"); return; }

    // I/O ports
    if (WITHIN(0x1F801000, 4096))
        { dbg("TODO: IO ports"); return; }
    if (WITHIN(0x9F801000, 4096))
        { dbg("TODO: IO ports"); return; }
    if (WITHIN(0xBF801000, 4096))
        { dbg("TODO: IO ports"); return; }

    // BIOS ROM
    if (WITHIN(0x1FC00000, 4096 * 1024) || WITHIN(0x9FC00000, 4096 * 1024) || WITHIN(0xBFC00000, 4096 * 1024))
        return;

    // MEMCONTROL
    if ((address >= 0x1f801000 && address <= 0x1f801000 + 36) || (address >= 0x1f801060 && address <= 0x1f801060 + 4))
    {
        dbg("TODO: memcontrol registers");
        if (value != 0) exit(1);
        return;
    }

    // CACHECONTROL
    if (WITHIN(0xFFFE0000, 512))
    {
        dbg("TODO: cache control registers", dbg::hex(address), dbg::hex(value));
        return;
    }

    printf("TODO: bus write with address 0x%X, value 0x%X\n", address, value);
    exit(1);
}

void Bus::write_u16(const u32 address, const u16 value)
{
    write_u8(address + 0, (u8)(value >> 0));
    write_u8(address + 1, (u8)(value >> 8));
}

void Bus::write_u32(const u32 address, const u32 value)
{
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