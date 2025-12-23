#pragma once

/*
        KUSEG     KSEG0     KSEG1
    00000000h 80000000h A0000000h  2048K  Main RAM (first 64K reserved for BIOS)
    1F000000h 9F000000h BF000000h  8192K  Expansion Region 1 (ROM/RAM)
    1F800000h 9F800000h    --      1K     Scratchpad (D-Cache used as Fast RAM)
    1F801000h 9F801000h BF801000h  4K     I/O Ports
    1F802000h 9F802000h BF802000h  8K     Expansion Region 2 (I/O Ports)
    1FA00000h 9FA00000h BFA00000h  2048K  Expansion Region 3 (SRAM BIOS region for DTL cards)
    1FC00000h 9FC00000h BFC00000h  512K   BIOS ROM (Kernel) (4096K max)
            FFFE0000h (in KSEG2)   0.5K   Internal CPU control registers (Cache Control)


    $00000000-$7FFFFFFF: KUSEG (2GB user memory)
    $80000000-$9FFFFFFF: KSEG0 (512MB cached kernel memory)
    $A0000000-$BFFFFFFF: KSEG1 (512MB uncached kernel memory)
    $C0000000-$FFFFFFFF: KSEG2 (1GB virtual memory)

    KUSEG, KSEG0 and KSEG1 are essentially mirrors of the same physical memory
    with different attributes. KSEG2 is unmapped, save for 0xFFFE0130 (the CPU
    cache control register).

    Only the first 512MB of KUSEG is mapped.
*/

class Bus
{
public:
    Bus(const std::string& bios_path);

    u8 read_u8(const u32 address);
    u16 read_u16(const u32 address);
    u32 read_u32(const u32 address);
    void write_u8(const u32 address, const u8 value);
    void write_u16(const u32 address, const u16 value);
    void write_u32(const u32 address, const u32 value);

private:
    void read_from_file(u8* buffer, const std::string& filename);

    u8 bios[4096 * 1024]; // 4 KB
    u8 ram[2 * 1024 * 1024]; // 2 MB
};