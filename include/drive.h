#pragma once

class Drive
{
public:
    Drive();
    bool clock();
    u8 read(const u32 address);
    void write(const u32 address, const u8 value);

private:
    // HSTS / ADDRESS
    u8 bank;
    bool data_request_pending = false;

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
    u8 hintsts;
    u8 hintmsk;

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
    FIFO results_fifo = {};
    FIFO commands_fifo = {};
    FIFO parameters_fifo = {};

    struct SeekLocation
    {
        u8 min;
        u8 sector;
        u8 frame;
    } seek_target, seek_data;

    // Some commands can send several responses
    enum class SecondResponse
    {
        None,
        GetID,
        SeekL,
        Pause
    } second_response;

    enum class ReadNMode
    {
        None,
        Interrupt,
        Data
    } read_n_mode;

    // Status code (stat)
    bool shell_open = true;
    bool reading_data_sector = false;
};