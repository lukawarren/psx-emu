#include "bus.h"
#include "cpu.h"

#define WITHIN(base, size) (physical_address >= base && physical_address < base + size)

Bus::Bus(CPU& cpu, const std::string& bios_path) : cpu(cpu)
{
    memset(bios, 0, sizeof(bios));
    memset(ram, 0, sizeof(ram));
    read_file(bios, 512 * 1024, bios_path);
}

void Bus::clock()
{
    // TODO: analyse priorities if multiple DMA channels are due
    //       (on a tie, take 0 as lowest and 6 as highest priority)

    // TOOD: abstract
    if (drive.clock())
        fire_interrupt(Interrupt::CDROM);

    // DMA has 0 as lowest priority, 6 as highest - TODO: sort by get_dma_priority(n) first
    int pending_channel = -1;
    for (int i = 6; i >= 0; i--)
    {
        if (get_dma_master_enable((u8)i) &&
            DMAChannelControl::transfer_due(dma_channel_control_channel[(u32)i]))
        {
            pending_channel = i;
            break;
        }
    }

    if (pending_channel != -1)
    {
        initialise_dma(
            (DMAChannel)pending_channel,
            DMAChannelControl(dma_channel_control_channel[(u32)pending_channel], (u8)pending_channel)
        );
    }
}

void Bus::sideload_exe(const std::string& path)
{
    const u8* exe = read_file(path);

    const uint32_t initial_pc =
        (u32)(exe[0x10]) |
        ((u32)(exe[0x11]) << 8) |
        ((u32)(exe[0x12]) << 16) |
        ((u32)(exe[0x13]) << 24);

    const uint32_t initial_r28 =
        (u32)(exe[0x14]) |
        ((u32)(exe[0x15]) << 8) |
        ((u32)(exe[0x16]) << 16) |
        ((u32)(exe[0x17]) << 24);

    const uint32_t exe_ram_addr =
        ((u32)(exe[0x18]) |
        ((u32)(exe[0x19]) << 8) |
        ((u32)(exe[0x1A]) << 16) |
        ((u32)(exe[0x1B]) << 24)) & 0x1FFFFF;

    const uint32_t exe_size_2kb =
        (u32)(exe[0x1C]) |
        ((u32)(exe[0x1D]) << 8) |
        ((u32)(exe[0x1E]) << 16) |
        ((u32)(exe[0x1F]) << 24);

    const uint32_t initial_sp =
        (u32)(exe[0x30]) |
        ((u32)(exe[0x31]) << 8) |
        ((u32)(exe[0x32]) << 16) |
        ((u32)(exe[0x33]) << 24);

    memcpy(ram + exe_ram_addr, &exe[2048], exe_size_2kb);
    cpu.configure_from_exe(initial_r28, initial_sp, initial_pc);

    delete[] exe;
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
        return drive.read(physical_address - 0x1F801800);

    dbg("TODO: unknown bus read", dbg::hex(address), dbg::hex(physical_address));
    return 0xff;
}

u16 Bus::read_u16(const u32 address)
{
    const u32 physical_address = get_physical_address(address);
    if (u8* ptr = get_bank_address(physical_address))
        return (u32)ptr[0] | ((u32)ptr[1] << 8);

    // I/O
    if (WITHIN(0x1F801000, 4096) || address == 0xFFFE0130)
        return io_read_u16(physical_address);

    dbg("TODO: unknown bus read", dbg::hex(address), dbg::hex(physical_address));
    return 0xff;
}

u32 Bus::read_u32(const u32 address)
{
    const u32 physical_address = get_physical_address(address);
    if (u8* ptr = get_bank_address(physical_address))
        return (u32)ptr[0] | ((u32)ptr[1] << 8) | ((u32)ptr[2] << 16) | ((u32)ptr[3] << 24);

    // I/O
    if (WITHIN(0x1F801000, 4096) || address == 0xFFFE0130)
        return io_read_u32(physical_address);

    dbg("TODO: unknown bus read", dbg::hex(address), dbg::hex(physical_address));
    return 0xff;
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

    // Scratchpad
    if (physical_address >= 0x1F800000 && physical_address < 0x1F800000 + 1024)
    {
        scratchpad[physical_address - 0x1F800000] = value;
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
    {
        drive.write(address - 0x1F801800, value);
        return;
    }

    dbg("TODO: unknown bus write", dbg::hex(address), dbg::hex(value));
}

void Bus::write_u16(const u32 address, const u16 value)
{
    const u32 physical_address = get_physical_address(address);

    // I/O
    if (WITHIN(0x1F801000, 4096))
    {
        io_write_u32(physical_address, value);
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
        io_write_u32(physical_address, value);
        return;
    }

    write_u8(physical_address + 0, (u8)(value >> 0));
    write_u8(physical_address + 1, (u8)(value >> 8));
    write_u8(physical_address + 2, (u8)(value >> 16));
    write_u8(physical_address + 3, (u8)(value >> 24));
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
        return 0xff;

    // SPU control registers
    if (address >= 0x1F801D80 && address <= 0x1F801DBC)
        return 0xff;

    // SPU reverb configuration area - NOTE: should be DFC, not DFE?
    if (address >= 0x1F801DC0 && address <= 0x1F801DFE)
        return 0xff;

    dbg("TODO: unrecognised I/O read", dbg::hex(address));
    return 0xff;
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
        return 0xff;

    // SPU control registers
    if (address >= 0x1F801D80 && address <= 0x1F801DBC)
        return 0xff;

    // SPU reverb configuration area - NOTE: should be DFC, not DFE?
    if (address >= 0x1F801DC0 && address <= 0x1F801DFE)
        return 0xff;

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
    if (address >= 0x1F801080 && address <= 0x1F8010E8)
    {
        const u32 offset = address & 0xF;
        const u32 channel = ((address & 0xF0) >> 4) - 0x8;

        switch (offset)
        {
            case 0: return dma_base_address_channel[channel];
            case 4: return dma_block_control_channel[channel];
            case 8: return dma_channel_control_channel[channel];
        }

        return 0;
    }

    dbg("TODO: unrecognised I/O read", dbg::hex(address));
    return 0xff;
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

    // I_STAT - NOTE: writing is to "acknowledge" (i.e. set to 0)
    if (address == 0x1F801070)
    {
        i_stat &= value;
        return;
    }

    // I_MASK
    if (address == 0x1F801074)
    {
        i_mask = value;
        return;
    }

    dbg("TODO: unrecognised I/O write", dbg::hex(address), dbg::hex(value));
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

    // Cache configuration register (TODO)
    if (address == 0xFFFE0130)
        return;

    // I_STAT - NOTE: writing is to "acknowledge"
    if (address == 0x1F801070)
    {
        i_stat &= value;
        return;
    }

    // I_MASK
    if (address == 0x1F801074)
    {
        i_mask = value;
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
    if (address >= 0x1F801080 && address <= 0x1F8010E8)
    {
        const u32 offset = address & 0xF;
        const u32 channel = ((address & 0xF0) >> 4) - 0x8;

        switch (offset)
        {
            case 0: dma_base_address_channel[channel] = value; break;
            case 4: dma_block_control_channel[channel] = value; break;
            case 8: dma_channel_control_channel[channel] = value; break;
        }

        return;
    }

    // GPU
    if (address == 0x1F801810) { gpu.send_gp0_command(value); return; }
    if (address == 0x1F801814) { gpu.send_gp1_command(value); return; }

    // Serial something or other
    if (address == 0x1F801040) return;
    if (address == 0x1F80104A) return;

    dbg("TODO: unrecognised I/O write", dbg::hex(address), dbg::hex(value));
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

    // Scratchpad
    if (physical_address >= 0x1F800000 && physical_address < 0x1F800000 + 1024)
        return &scratchpad[physical_address - 0x1F800000];

    return nullptr;
}

std::pair<std::ifstream, std::streamsize> Bus::read_file(const std::string& filename, const size_t max_size) const
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

    if (max_size != 0 && (size_t)file_length > max_size)
        throw std::runtime_error(filename + " is too large");

    return { std::move(file), file_length };
}

void Bus::read_file(u8* buffer, const size_t max_size, const std::string& filename) const
{
    auto file = read_file(filename, max_size);
    file.first.read(reinterpret_cast<char*>(buffer), file.second);
    file.first.close();
}

u8* Bus::read_file(const std::string& filename) const
{
    auto file = read_file(filename, 0);
    u8* buffer = new u8[file.second];
    file.first.read(reinterpret_cast<char*>(buffer), file.second);
    file.first.close();
    return buffer;
}

void Bus::initialise_dma(const DMAChannel channel, const DMAChannelControl control)
{
    switch (control.transfer_mode)
    {
        case DMAChannelControl::TransferMode::Burst: dma_burst(channel, control); break;
        case DMAChannelControl::TransferMode::LinkedList: dma_linked_list(channel, control); break;
        case DMAChannelControl::TransferMode::Slice: dma_slice(channel, control); break;
    }

    // Mark DMA as finished
    dma_channel_control_channel[(u32)channel] = DMAChannelControl::with_transfer_finished(
        dma_channel_control_channel[(u32)channel]
    );
}

void Bus::dma_slice(const DMAChannel channel, const DMAChannelControl control)
{
    const u32 n = (u32)channel;
    const u32 block_size = dma_block_control_channel[n] & 0xFFFF;
    const u32 blocks = dma_block_control_channel[n] >> 16;
    const u64 length = blocks * block_size;

    // Sanitise address (let RAM wrap around)
    const u32 address = dma_base_address_channel[n] & 0x1FFFFC;

    // Meant to pass N blocks, block by block, to device
    // TODO: if block_size > length device can take, presumably drop
    // TODO: respect DREQ
    if (channel == DMAChannel::GPU && control.transfer_direction_is_to_device)
    {
        for (u64 i = 0; i < length; i++)
            gpu.dma_send(read_u32((u32)(address + i * 4)));
    }
    else
    {
        dbg("TODO: unsupported DMA configuration", n, control.transfer_direction_is_to_device);
        exit(1);
    }

    // Set amount of blocks remaining to 0 in block control register
    dma_block_control_channel[n] &= 0x0000FFFF;

    // Set relevant MADR to the end marker
    dma_base_address_channel[n] = 0xFFFFFF;
}

void Bus::dma_linked_list(const DMAChannel channel, const DMAChannelControl control)
{
    // Sanitise address (let RAM wrap around)
    u32 header_address = dma_base_address_channel[(u32)channel] & 0x1FFFFC;

    while (true)
    {
        // Linked list DMA - parse header
        const u32 header = read_u32(header_address);
        const u32 next_node_address = header & 0b111111111111111111111111;
        const u32 extra_words = header >> 24;
        const bool is_end_marker = next_node_address == 0xFFFFFF;

        if (channel == DMAChannel::GPU && control.transfer_direction_is_to_device)
        {
            for (u32 i = 0; i < extra_words; i++)
                gpu.dma_send(read_u32(header_address + (i + 1) * 4));
        }
        else
        {
            dbg("TODO: unsupported DMA configuration", (u32)channel, control.transfer_direction_is_to_device);
            exit(1);
        }

        if (is_end_marker)
            break;

        header_address = next_node_address;
    }
}

void Bus::dma_burst(const DMAChannel channel, const DMAChannelControl control)
{
    // TODO: respect DREQ - right now just assume the target is ready
    const u32 n = (u32)channel;

    if (control.burst_mode_enables_chopping)
    {
        dbg("TODO: burst mode wants to support chopping + update MADR");
        exit(1);
    }

    if (control.transfer_direction_is_to_device)
    {
        dbg("TODO: unsupported DMA configuration (RAM to device)");
        exit(1);
    }

    u32 address = dma_base_address_channel[n];
    u32 words = dma_block_control_channel[n] & 0xFFFF;
    if (words == 0) words = 0x10000;

    while (words > 0)
    {
        // Sanitise address (let RAM wrap around)
        const u32 addr = address & 0x1FFFFC;

        if (channel == DMAChannel::OTC)
        {
            // Last entry table marker, else pointer to previous entry
            if (words == 1) write_u32(addr, 0xFFFFFF);
            else write_u32(addr, (address - 4) & 0x1FFFFF);
        }
        else if (channel == DMAChannel::CDROM)
        {
            // Read from RDDATA - CDROM bus is only 8 bits wide; bus handles conversion
            u32 data = 0;
            data |= (u32)(drive.read(2) << 0);
            data |= (u32)(drive.read(2) << 8);
            data |= (u32)(drive.read(2) << 16);
            data |= (u32)(drive.read(2) << 24);
            write_u32(addr, data);
        }
        else
        {
            dbg("TODO: unsupported DMA channel", n);
            exit(1);
        }

        if (control.madr_increment_per_step) address -= 4; else address += 4;
        words -= 1;
    }

    dma_channel_control_channel[n] = DMAChannelControl::with_transfer_finished(dma_base_address_channel[n]);

    // Set relevant MADR to the end marker
    dma_base_address_channel[n] = 0xFFFFFF;
}