#pragma once
#include "cue.h"

class Drive
{
public:
    Drive();

    bool clock();
    u8 read(const u32 address);
    void write(const u32 address, const u8 value);

private:
    // HINTSTS interrupt flags (bits 0-2 inclusive)
    enum class InterruptType
    {
        NoInterruptPending = 0,
        DataReady = 1,
        Complete = 2,
        Acknowledge = 3,
        DataEnd = 4,
        DiskError = 5
    };
    u8 interrupt_status;

    // HINTMSK (interrupt mask)
    u8 interrupt_mask;

    // HSTS
    struct FifoStatus
    {
        u8 bank;                      // RA
        bool adpcm_busy;              // ADPBUSY
        bool parameter_fifo_empty;    // PRMEMPT; 1 = parameter FIFO empty
        bool parameter_fifo_not_full; // PRMWRDY; 1 = parameter FIFO not full
        bool result_data_ready;       // RSLRRDY; 1 = result FIFO not empty;
        bool data_request;            // DRQSTS; 1 = one or more RDDATA reads or WRDATA writes pending
        bool busy;                    // BUSYSTS; 1 = HC05 busy ACK'ing command

        FifoStatus()
        {
            bank = 0;
            parameter_fifo_empty = true;
            parameter_fifo_not_full = true;
            result_data_ready = false;
            data_request = false;
            busy = false;
        }

        u8 to_u8() const
        {
            return (bank & 0b11) |
                (u8)(adpcm_busy << 2) |
                (u8)(parameter_fifo_empty << 3) |
                (u8)(parameter_fifo_not_full << 4) |
                (u8)(result_data_ready << 5) |
                (u8)(data_request << 6) |
                (u8)(busy << 7);
        }
    } fifo_status = {};

    // Status code (stat)
    struct Status
    {
        // Bits 0-4 inclusive
        bool error = false;
        bool motor_on = false;
        bool seek_error = false;
        bool get_id_error = false;
        bool shell_open = false;

        // Play, seek and read bits (7, 6 and 5)
        enum class Mode
        {
            None,
            Play,
            Seek,
            Read
        } mode = Mode::None;

        u8 to_u8() const
        {
            u8 value =
                ((u8)error << 0) |
                ((u8)motor_on << 1) |
                ((u8)seek_error << 2) |
                ((u8)get_id_error << 3) |
                ((u8)shell_open << 4);

            switch (mode)
            {
                case Mode::Play: value |= 1 << 7; break;
                case Mode::Seek: value |= 1 << 6; break;
                case Mode::Read: value |= 1 << 5; break;
                default:                          break;
            }

            return value;
        }

    } status = {};

    // Mode
    struct Mode
    {
        bool cdda;
        bool auto_pause;
        bool report_interrupt_enable;
        bool xa_filter;
        bool ignore_bit;
        bool sector_size;
        bool xa_adpcm;
        bool double_speed;

        Mode()
        {
            // Default value, at least from Init command
            from_u8(0x20);
        }

        void from_u8(const u8 value)
        {
            cdda                    = ((value >> 0) & 0b1) != 0;
            auto_pause              = ((value >> 1) & 0b1) != 0;
            report_interrupt_enable = ((value >> 2) & 0b1) != 0;
            xa_filter               = ((value >> 3) & 0b1) != 0;
            ignore_bit              = ((value >> 4) & 0b1) != 0;
            sector_size             = ((value >> 5) & 0b1) != 0;
            xa_adpcm                = ((value >> 6) & 0b1) != 0;
            double_speed            = ((value >> 7) & 0b1) != 0;
        }

        u8 to_u8() const
        {
            return
                ((u8)cdda << 0) |
                ((u8)auto_pause << 1) |
                ((u8)report_interrupt_enable << 2) |
                ((u8)xa_filter << 3) |
                ((u8)ignore_bit << 4) |
                ((u8)sector_size << 5) |
                ((u8)xa_adpcm << 6) |
                ((u8)double_speed << 7);
        }
    } mode = {};

    struct FIFO
    {
        void push(const u8 data)
        {
            if (is_full()) return;
            buffer[elements++] = data;
        }

        u8 pop()
        {
            if (is_empty()) return 0;
            u8 value = buffer[0];
            for (u8 i = 0; i < elements - 1; i++)
                buffer[i] = buffer[i + 1];
            elements--;
            return value;
        }

        void clear() { elements = 0; }
        bool is_empty() { return elements == 0; }
        bool is_full() { return elements == 16; }

    private:
        u8 elements = 0;
        u8 buffer[16];
    };

    // FIFOs
    FIFO result_fifo = {};
    FIFO parameter_fifo = {};

    // Need to delay processing of commands, and handle "multi-stage commands"
    u32 command_delay_timer = 0;
    u32 read_play_delay_timer = 0;
    std::optional<u8> command;
    enum class CommandState
    {
        None,
        GetID,
        SeekL,
        Pause,
        Init
    } command_state = CommandState::None;

    // SetLoc data
    struct SetLoc
    {
        u8 minutes = 0;
        u8 seconds = 0;
        u8 sector = 0;
    } set_loc_data = {};

    // Seeking
    u32 seek_sector = 0;

    // Reading
    std::vector<u8> read_buffer;
    u32 read_buffer_length;
    u32 read_buffer_index = 0;

    void continue_previous_command();
    void on_command(const u8 opcode);

    void reset_parameter_fifo();
    void push_parameter(const u8 value);
    void push_command(const u8 value);
    void push_result(const u8 value);
    void push_status();
    u8 pop_parameter();
    u8 pop_result();
    u8 pop_data();

    void raise_interrupt(const InterruptType interrupt);

    void on_seek();
    void on_read();

    u8 from_bcd(const u8 bcd) const
    {
        const int hi = (bcd >> 4) & 0xf;
        const int lo = bcd & 0xf;
        return (u8)(hi * 10 + lo);
    }

    u8 to_bcd(const u8 b) const
    {
        return ((b / 10) << 4) | (b % 10);
    }

    Cue cue;
};