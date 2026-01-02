#include "drive.h"

Drive::Drive()
{
    // Bits 5-7 are always 1
    hintsts = 0b11100000;
}

bool Drive::clock()
{
    const auto int2 = [&]() { hintsts = (hintsts & 0b11111000) | (u8)InterruptType::Complete; };
    const auto int3 = [&]() { hintsts = (hintsts & 0b11111000) | (u8)InterruptType::Acknowledge; };
    const auto int5 = [&]() { hintsts = (hintsts & 0b11111000) | (u8)InterruptType::DiskError; };
    const auto stat = [&]() -> u8
    {
        // Motor on; bit 4 is shell open
        return 0b10 | ((u8)shell_open << 4);
    };
    const auto int3_stat = [&]()
    {
        results_fifo.push(stat());
        int3();
    };

    // Check for pending interrupt
    if ((hintmsk & hintsts) != 0)
        return true;

    if (second_response == SecondResponse::GetID)
    {
        // Return licensed string (America / NTSC)
        results_fifo.push(0x02);
        results_fifo.push(0x00);
        results_fifo.push(0x20);
        results_fifo.push(0x00);
        results_fifo.push('S');
        results_fifo.push('C');
        results_fifo.push('E');
        results_fifo.push('A');
        int2();
    }
    else if (second_response == SecondResponse::SeekL)
    {
        dbg("stop any reading or pending ReadN or ReadS");
        results_fifo.push(stat());
        int2();
    }

    if (second_response != SecondResponse::None)
    {
        second_response = SecondResponse::None;
        return false;
    }

    if (commands_fifo.is_empty())
        return false;

    const u8 command = commands_fifo.pop();

    switch (command)
    {
        case 0x00:
            // Unused
            results_fifo.push(0x11);
            results_fifo.push(0x40);
            int5();
            break;

        case 0x01:
            // NOP - resets shell open status
            shell_open = false;
            int3_stat();
            break;

        case 0x02:
            // SetLoc
            seek_target.min = parameters_fifo.pop();
            seek_target.sec = parameters_fifo.pop();
            seek_target.frame = parameters_fifo.pop();
            int3_stat();
            break;

        case 0x06:
            // ReadN
            dbg("TODO: read");
            exit(1);
            break;

        case 0x0E:
        {
            // Setmode
            const u8 mode = parameters_fifo.pop();
            dbg("TODO: setmode (including sector size)", dbg::bin(mode));
            if ((mode & 0b10) != 0) dbg("TODO: auto-pause");
            if ((mode & 0b10000) != 0) dbg("TODO: ignore bit");
            int3_stat();
            break;
        }

        case 0x15:
            // SeekL
            seek_data = seek_target;
            second_response = SecondResponse::SeekL;
            int3_stat();
            break;

        case 0x19:
        {
            // Test / "sub_function"
            const u8 sub_function = parameters_fifo.pop();
            if (sub_function != 0x20) dbg("TODO: unknown CDROM sub-function");

            // 0x20: Get CDROM BIOS date / version
            results_fifo.push(94);  // Year
            results_fifo.push(9);   // Month
            results_fifo.push(25);  // Day
            results_fifo.push(192); // Version
            int3();
            break;
        }

        case 0x1A:
        {
            // GetID; signal that we're licensed
            int3_stat();
            second_response = SecondResponse::GetID;
            break;
        }

        default:
            dbg("TODO: unknown CDROM command", dbg::hex(command));
            exit(1);
    }

    return false;
}

u8 Drive::read(const u32 address)
{
    switch (address)
    {
        case 0:
            // HSTS
            dbg("TODO: rest of HSTS bits");
            return
                (bank & 0b11) |
                (parameters_fifo.is_empty() ? 0b1000 : 0) |
                (parameters_fifo.is_full() ? 0 : 0b100000) |
                (results_fifo.is_empty() ? 0 : 0b100000);

        case 1:
            // RESULT
            return results_fifo.pop();

        case 2:
            dbg("RDDATA read");
            break;

        case 3:
            // HINTMSK - bits 5-7 reserved and always read as 1
            if (bank == 0 || bank == 2) return hintmsk | 0b11100000;

            // HINTSTS - force that audio ran out of sectors to play and is ready
            else return hintsts | 0b11000;
    }

    return 0;
}

void Drive::write(const u32 address, const u8 value)
{
    switch (address)
    {
        case 0:
            // ADDRESS; read-only save for bank field
            bank = value & 0b11;
            break;

        case 1:
            switch (bank)
            {
                case 0:
                    // COMMAND
                    commands_fifo.push(value);
                    break;

                case 1:
                    dbg("WRDATA write");
                    break;

                case 2:
                    dbg("CI write");
                    break;

                case 3:
                    dbg("ATV2 write");
                    break;
            }
            break;

        case 2:
            switch (bank)
            {
                case 0:
                    // PARAMETER
                    parameters_fifo.push(value);
                    break;

                case 1:
                    // HINTMSK - bits 5-7 always 0 when written
                    hintmsk = value & 0b00011111;
                    break;

                case 2:
                    dbg("AVT0 write");
                    break;

                case 3:
                    dbg("ATV3 write");
                    break;
            }
            break;

        case 3:
            switch (bank)
            {
                case 0:
                    dbg("HCHPCTL write");
                    break;

                case 1:
                    // HCLRCTL; 1 means "do something"
                    if ((value >> 0) & 0b1) hintsts &= (u8)~(1 << 0);
                    if ((value >> 1) & 0b1) hintsts &= (u8)~(1 << 1);
                    if ((value >> 2) & 0b1) hintsts &= (u8)~(1 << 2);
                    if ((value >> 3) & 0b1) hintsts &= (u8)~(1 << 3);
                    if ((value >> 4) & 0b1) hintsts &= (u8)~(1 << 4);
                    if ((value >> 5) & 0b1) dbg("TODO: clear sound map XA-ADPCM buffer");
                    if ((value >> 6) & 0b1) parameters_fifo.clear();
                    if ((value >> 7) & 0b1) dbg("TODO: reset decoder chip");

                    // If we acknowledged, drain result FIFO
                    if ((value & 0b11111) != 0)
                    {
                        results_fifo.clear();
                        dbg("TODO: if pending command, send to controller");
                    }
                    break;

                case 2:
                    dbg("AVT1 write");
                    break;

                case 3:
                    dbg("ADPCTL write");
                    break;
            }
            break;
    }
}
