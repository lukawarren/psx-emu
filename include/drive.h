#pragma once

class Drive
{
public:
    Drive();
    void clock();
    u8 read(const u32 address);
    void write(const u32 address, const u8 value);

private:
    // HSTS / ADDRESS
    u8 bank;

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
};