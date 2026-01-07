#pragma once
#include "gpu.h"
#include "interrupts.h"
#include "drive.h"

class CPU;

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
    Bus(CPU& cpu, const std::string& bios_path);
    void clock();
    void sideload_exe(const std::string& path);

    u8 read_u8(const u32 address);
    u16 read_u16(const u32 address);
    u32 read_u32(const u32 address);
    void write_u8(const u32 address, const u8 value);
    void write_u16(const u32 address, const u16 value);
    void write_u32(const u32 address, const u32 value);

    inline u16 interrupt_pending() const { return (i_stat & i_mask) != 0; }

    void fire_interrupt(const Interrupt interrupt)
    {
        i_stat |= 1 << (u32)interrupt;
    }

    u32 i_stat; // Interrupt status register
    u32 i_mask; // Interrupt mask register
    u32 timer_0;
    u32 timer_1;
    u32 timer_2;
    u32 timer_0_target;
    u32 timer_1_target;
    u32 timer_2_target;
    u32 dma_control = 0x07654321; // Initial value on reset
    u32 dma_interrupt;

    u32 dma_base_address_channel[7];
    u32 dma_block_control_channel[7];
    u32 dma_channel_control_channel[7];
    enum class DMAChannel
    {
        RAMToMDEC = 0,
        MDECToRAM = 1,
        GPU = 2,
        CDROM = 3,
        SPU = 4,
        PIO = 5,
        OTC = 6
    };

    GPU gpu;
    Drive drive;

private:
    u16 io_read_u16(const u32 address);
    u32 io_read_u32(const u32 address);
    void io_write_u16(const u32 address, const u16 value);
    void io_write_u32(const u32 address, const u32 value);
    u8* get_bank_address(const u32 address);
    u32 get_physical_address(const u32 address) const;

    u8 bios[4096 * 1024]; // 4 KB
    u8 ram[2 * 1024 * 1024]; // 2 MB
    u8 scratchpad[1024]; // 1 KB
    CPU& cpu;

    inline u8 get_dma_priority(const u8 channel) const
    {
        // Bits 0-2, 4-6, ..., 24-26 (for channel 6)
        return (dma_control >> (channel * 4)) & 0b111;
    }

    inline bool get_dma_master_enable(const u8 channel) const
    {
        // Bits 3, 7, 11, 15, ..., 27 (for channel 6)
        return ((dma_control >> (channel * 4 + 3)) & 0b1) != 0;
    }

    struct DMAChannelControl
    {
        enum class TransferMode
        {
            Burst = 0,
            Slice = 1,
            LinkedList = 2
        };

        bool transfer_direction_is_to_device; // 0 = device to RAM, 1 = RAM to device
        bool madr_increment_per_step; // 0 = +4, 1 = -4
        bool burst_mode_enables_chopping;
        TransferMode transfer_mode;
        u8 chopping_dma_window_size;
        u8 chopping_cpu_window_size;
        bool start_transfer;
        bool force_start_without_dreq;
        bool mode_modifier;

        DMAChannelControl() {}

        DMAChannelControl(const u32 control, const u8 channel)
        {
            if (channel == 6)
            {
                // Only bits 24, 28 and 30 are writable (ignore 30)
                start_transfer = (control >> 24) & 0b1;
                force_start_without_dreq = (control >> 28) & 0b1;

                // All others are 0...
                transfer_direction_is_to_device = 0;
                burst_mode_enables_chopping = 0;
                transfer_mode = TransferMode::Burst;
                chopping_dma_window_size = 0;
                chopping_cpu_window_size = 0;
                mode_modifier = 0;

                // Except bit 1, which is always 1 (i.e. always MADR increment)
                madr_increment_per_step = 1;
            }
            else
            {
                transfer_direction_is_to_device = (control >> 0) & 0b1;
                madr_increment_per_step = (control >> 1) & 0b1;
                burst_mode_enables_chopping = (control >> 8) & 0b1;
                transfer_mode = TransferMode((control >> 9) & 0b11);
                chopping_dma_window_size = 1 << ((control >> 16) & 0b111);
                chopping_cpu_window_size = 1 << ((control >> 20) & 0b111);
                start_transfer = (control >> 24) & 0b1;
                force_start_without_dreq = (control >> 28) & 0b1;
                mode_modifier = (control >> 29) & 0b1;
            }
        }

        static inline bool transfer_due(const u32 control)
        {
            return (control & (1 << 24)) != 0;
        }

        static inline bool with_transfer_finished(const u32 control)
        {
            // Clear "start transfer" and "force transfer start" bits
            return control & ~((1U << 24) | (1U << 28));
        }
    };

    void initialise_dma(const DMAChannel channel, const DMAChannelControl control);
    void dma_slice(const DMAChannel channel, const DMAChannelControl control);
    void dma_linked_list(const DMAChannel channel, const DMAChannelControl control);
    void dma_burst(const DMAChannel channel, const DMAChannelControl control);
};