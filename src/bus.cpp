#include "bus.h"

#define WITHIN(base, size) (physical_address >= base && physical_address < base + size)

Bus::Bus(const std::string& bios_path)
{
    memset(bios, 0, sizeof(bios));
    memset(ram, 0, sizeof(ram));
    read_from_file(bios, bios_path);
}

void Bus::clock()
{
    // TODO: analyse priorities if multiple DMA channels are due
    //       (on a tie, take 0 as lowest and 6 as highest priority)

    // TOOD: abstract

    if (get_dma_master_enable(2) && DMAChannelControl::transfer_due(dma_channel_control_channel_2))
    {
        // TODO: respect DREQ - right now just assume the target is ready
        DMAChannelControl control(dma_channel_control_channel_2, 2);

        if (!control.transfer_direction_is_to_device || (
            control.transfer_mode != DMAChannelControl::TransferMode::LinkedList &&
            control.transfer_mode != DMAChannelControl::TransferMode::Slice
        ))
        {
            dbg(
                "TODO: unsupported DMAChannelControl configuration",
                control.transfer_direction_is_to_device,
                (u32)control.transfer_mode
            );
            exit(1);
        }

        if (control.transfer_mode == DMAChannelControl::TransferMode::Slice)
        {
            const u32 block_size = dma_block_control_channel_2 & 0xFFFF;
            const u32 blocks = dma_block_control_channel_2 >> 16;
            const u64 length = blocks * block_size;

            // Sanitise address (let RAM wrap around)
            const u32 address = dma_base_address_channel_2 & 0x1FFFFC;

            // Meant to pass N blocks, block by block, to device
            // TODO: if block_size > length device can take, presumably drop
            // TODO: respect DREQ
            for (u64 i = 0; i < length; i++)
                gpu.dma_send(read_u32((u32)(address + i * 4)));

            // Set amount of blocks remaining to 0 in block control register
            dma_block_control_channel_2 &= 0x0000FFFF;

            // Set relevant MADR to the end marker
            dma_base_address_channel_2 = 0xFFFFFF;
        }
        else
        {
            // Sanitise address (let RAM wrap around)
            u32 header_address = dma_base_address_channel_2 & 0x1FFFFC;

            while (true)
            {
                // Linked list DMA - parse header
                const u32 header = read_u32(header_address);
                const u32 next_node_address = header & 0b111111111111111111111111;
                const u32 extra_words = header >> 24;
                const bool is_end_marker = next_node_address == 0xFFFFFF;

                for (u32 i = 0; i < extra_words; i++)
                    gpu.dma_send(read_u32(header_address + (i + 1) * 4));

                if (is_end_marker)
                    break;

                header_address = next_node_address;
            }
        }

        dma_channel_control_channel_2 = DMAChannelControl::with_transfer_finished(dma_base_address_channel_2);
    }

    // Channel 6 is just used by the DMA to generate a linked-list for use by channel 2
    else if (get_dma_master_enable(6) && DMAChannelControl::transfer_due(dma_channel_control_channel_6))
    {
        // TODO: respect DREQ - right now just assume the target is ready
        DMAChannelControl control(dma_channel_control_channel_6, 6);

        if (control.transfer_direction_is_to_device ||
            control.transfer_mode != DMAChannelControl::TransferMode::Burst)
        {
            dbg(
                "TODO: unsupported DMAChannelControl configuration",
                control.transfer_direction_is_to_device,
                (u32)control.transfer_mode
            );
            exit(1);
        }

        if (control.burst_mode_enables_chopping)
        {
            dbg("TODO: burst mode wants to support chopping + update MADR");
            exit(1);
        }

        u32 address = dma_base_address_channel_6;
        u32 words = dma_block_control_channel_6 & 0xFFFF;
        if (words == 0) words = 0x10000;

        while (words > 0)
        {
            // Sanitise address (let RAM wrap around)
            const u32 addr = address & 0x1FFFFC;

            // Last entry table marker, else pointer to previous entry
            if (words == 1) write_u32(addr, 0xFFFFFF);
            else write_u32(addr, (address - 4) & 0x1FFFFF);

            if (control.madr_increment_per_step) address -= 4; else address += 4;
            words -= 1;
        }

        dma_channel_control_channel_6 = DMAChannelControl::with_transfer_finished(dma_base_address_channel_6);

        // Set relevant MADR to the end marker
        dma_base_address_channel_6 = 0xFFFFFF;
    }
}

u8 Bus::read_u8(const u32 address)
{
    const u32 physical_address = get_physical_address(address);
    if (u8* ptr = get_bank_address(physical_address))
        return *ptr;

    // Expansion 1
    if (WITHIN(0x1F000000, 8192 * 1024))
        return 0xff;

    // Expansion 2
    if (WITHIN(0x1F802000, 8 * 1024))
        return 0xff;

    // CD-ROM
    if (WITHIN(0x1F801800, 4))
        return 0;

    dbg("TODO: unknown bus read", dbg::hex(address), dbg::hex(physical_address));
    exit(1);
}

u16 Bus::read_u16(const u32 address)
{
    const u32 physical_address = get_physical_address(address);
    if (u8* ptr = get_bank_address(physical_address))
        return (u32)ptr[0] | ((u32)ptr[1] << 8);

    // I/O
    if (WITHIN(0x1F801000, 4096) || address == 0xFFFE0130)
        return io_read_u16(address);

    dbg("TODO: unknown bus read", dbg::hex(address), dbg::hex(physical_address));
    exit(1);
}

u32 Bus::read_u32(const u32 address)
{
    const u32 physical_address = get_physical_address(address);
    if (u8* ptr = get_bank_address(physical_address))
        return (u32)ptr[0] | ((u32)ptr[1] << 8) | ((u32)ptr[2] << 16) | ((u32)ptr[3] << 24);

    // I/O
    if (WITHIN(0x1F801000, 4096) || address == 0xFFFE0130)
        return io_read_u32(address);

    dbg("TODO: unknown bus read", dbg::hex(address), dbg::hex(physical_address));
    exit(1);
}

void Bus::write_u8(const u32 address, const u8 value)
{
    const u32 physical_address = get_physical_address(address);

    // Main RAM
    if (physical_address < 2048 * 1024)
    {
        ram[physical_address] = value;
        return;
    }

    // Expansion 1
    if (WITHIN(0x1F000000, 8192 * 1024))
        return;

    // Expansion 2
    if (WITHIN(0x1F802000, 8 * 1024))
        return;

    // BIOS ROM
    if (WITHIN(0x1FC00000, 4096 * 1024))
        return;

    // CD-ROM
    if (WITHIN(0x1F801800, 4))
        return;

    dbg("TODO: unknown bus write", dbg::hex(address), dbg::hex(value));
    exit(1);
}

void Bus::write_u16(const u32 address, const u16 value)
{
    const u32 physical_address = get_physical_address(address);

    // I/O
    if (WITHIN(0x1F801000, 4096))
    {
        io_write_u32(address, value);
        return;
    }

    write_u8(physical_address + 0, (u8)(value >> 0));
    write_u8(physical_address + 1, (u8)(value >> 8));
}

void Bus::write_u32(const u32 address, const u32 value)
{
    const u32 physical_address = get_physical_address(address);

    // I/O
    if (WITHIN(0x1F801000, 4096) || physical_address == 0xFFFE0130)
    {
        io_write_u32(address, value);
        return;
    }

    write_u8(physical_address + 0, (u8)(value >> 0));
    write_u8(physical_address + 1, (u8)(value >> 8));
    write_u8(physical_address + 2, (u8)(value >> 16));
    write_u8(physical_address + 3, (u8)(value >> 24));
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
    // I_STAT
    if (address == 0x1F801070)
        return (u16)i_stat;

    // I_MASK
    if (address == 0x1F801074)
        return (u16)i_mask;

    // SPU voice registers
    if (address >= 0x1F801C00 && address <= 0x1F801D7E)
        return 0;

    // SPU control registers
    if (address >= 0x1F801D80 && address <= 0x1F801DBC)
        return 0;

    // SPU reverb configuration area - NOTE: should be DFC, not DFE?
    if (address >= 0x1F801DC0 && address <= 0x1F801DFE)
        return 0;

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
    if (address == 0x1F801100) return timer_0;
    if (address == 0x1F801110) return timer_1;
    if (address == 0x1F801120) return timer_2;

    // SPU voice registers
    if (address >= 0x1F801C00 && address <= 0x1F801D7E)
        return 0;

    // SPU control registers
    if (address >= 0x1F801D80 && address <= 0x1F801DBC)
        return 0;

    // SPU reverb configuration area - NOTE: should be DFC, not DFE?
    if (address >= 0x1F801DC0 && address <= 0x1F801DFE)
        return 0;

    // DMA control
    if (address == 0x1F8010F0)
        return dma_control;

    // DMA interrupt
    if (address == 0x1F8010F4)
        return dma_interrupt;

    // GPU
    if (address == 0x1F801810) return gpu.get_gpu_read();
    if (address == 0x1F801814) return gpu.get_gpu_stat();

    // DMA channels
    if (address == 0x1F8010A0) { return dma_base_address_channel_2; }
    if (address == 0x1F8010A4) { return dma_block_control_channel_2; }
    if (address == 0x1F8010A8) { return dma_channel_control_channel_2; }
    if (address == 0x1F8010E0) { return dma_base_address_channel_6; }
    if (address == 0x1F8010E4) { return dma_block_control_channel_6; }
    if (address == 0x1F8010E8) { return dma_channel_control_channel_6; }

    dbg("TODO: unrecognised I/O read", dbg::hex(address));
    exit(1);
}

void Bus::io_write_u16(const u32 address, const u16 value)
{
    // SPU voice registers
    if (address >= 0x1F801C00 && address <= 0x1F801D7E)
        return;

    // SPU control registers (some are 16-bits wide as opposed to 32 so skip here)
    if (address >= 0x1F801D80 && address <= 0x1F801DBC)
        return;

    // SPU reverb configuration area - NOTE: should be DFC, not DFE?
    if (address >= 0x1F801DC0 && address <= 0x1F801DFE)
        return;

    // I_STAT - NOTE: writing is to "acknowledge"
    if (address == 0x1F801070)
    {
        i_stat = value;
        if (value != 0)
            dbg("TODO: unsupported i_stat value", dbg::hex(value));
        return;
    }

    // I_MASK
    if (address == 0x1F801074)
    {
        i_mask = value;
        if (value != 0)
            dbg("TODO: unsupported i_mask value", dbg::hex(value));
        return;
    }

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

    // SPU voice registers
    if (address >= 0x1F801C00 && address <= 0x1F801D7E)
        return;

    // SPU control registers
    if (address >= 0x1F801D80 && address <= 0x1F801DBC)
        return;

    // SPU reverb configuration area - NOTE: should be DFC, not DFE?
    if (address >= 0x1F801DC0 && address <= 0x1F801DFE)
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
            dbg("TODO: unsupported i_stat value", dbg::hex(value));
        return;
    }

    // I_MASK
    if (address == 0x1F801074)
    {
        i_mask = value;
        if (value != 0)
            dbg("TODO: unsupported i_mask value", dbg::hex(value));
        return;
    }

    // Timer counter modes
    if (address == 0x1F801104)
    {
        if (value != 0)
            dbg("TODO: unsupported timer 0 counter mode", dbg::hex(value));
        timer_0 = 0;
        return;
    }
    if (address == 0x1F801114)
    {
        if (value != 0)
            dbg("TODO: unsupported timer 1 counter mode", dbg::hex(value));
        timer_1 = 0;
        return;
    }
    if (address == 0x1F801124)
    {
        if (value != 0)
            dbg("TODO: unsupported timer 2 counter mode", dbg::hex(value));
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

    // DMA control
    if (address == 0x1F8010F0)
    {
        dma_control = value;
        return;
    }

    // DMA interrupt
    if (address == 0x1F8010F4)
    {
        // Writing bits 24-30 will set them to 0 if a 1 is written
        dma_interrupt = value & 0b01111111000000000000000000000000;
        return;
    }

    // DMA channels
    if (address == 0x1F8010A0) { dma_base_address_channel_2 = value; return; }
    if (address == 0x1F8010A4) { dma_block_control_channel_2 = value; return; }
    if (address == 0x1F8010A8) { dma_channel_control_channel_2 = value; return; }
    if (address == 0x1F8010E0) { dma_base_address_channel_6 = value; return; }
    if (address == 0x1F8010E4) { dma_block_control_channel_6 = value; return; }
    if (address == 0x1F8010E8) { dma_channel_control_channel_6 = value; return; }

    // GPU
    if (address == 0x1F801810) { gpu.send_gp0_command(value); return; }
    if (address == 0x1F801814) { gpu.send_gp1_command(value); return; }

    dbg("TODO: unrecognised I/O write", dbg::hex(address), dbg::hex(value));
    exit(1);
}

u32 Bus::get_physical_address(const u32 address) const
{
    const static u32 region_mask[8] {
        // KUSEG: 2048MB
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF , 0xFFFFFFFF,
        // KSEG0: 512MB
        0x7FFFFFFF ,
        // KSEG1: 512MB
        0x1FFFFFFF ,
        // KSEG2: 1024MB
        0xFFFFFFFF, 0xFFFFFFFF
    };

    return address & region_mask[address >> 29];
}

u8* Bus::get_bank_address(const u32 physical_address)
{
    // Main RAM
    if (physical_address < 2 * 1024 * 1024)
        return &ram[physical_address];

    // BIOS ROM
    if (physical_address >= 0x1FC00000 && physical_address < 0x1FC00000 + 512 * 1024)
        return &bios[physical_address & 0x3FFFFF];

    return nullptr;
}
