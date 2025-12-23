#include "bus.h"

Bus::Bus(const std::string& bios_path)
{
    read_from_file(bios, bios_path);
}

u8 Bus::read_u8(const u32 address)
{
    if (address >= 0xBFC00000 && address <= 0xBFC00000 + 4096 * 1024)
    {
        return bios[address - 0xBFC00000];
    }

    dbg("TODO: bus", dbg::hex(address));
    exit(1);
}

u16 Bus::read_u16(const u32 address)
{
    return (u32)read_u8(address) | ((u32)read_u8(address + 1) << 8);
}

u32 Bus::read_u32(const u32 address)
{
    return (u32)read_u8(address) |
           ((u32)read_u8(address + 1) << 8) |
           ((u32)read_u8(address + 2) << 16) |
           ((u32)read_u8(address + 3) << 24);
}

void Bus::write_u8(const u32 address, const u8 value)
{
    // dbg("TODO: bus write", dbg::hex(address), dbg::hex(value));
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