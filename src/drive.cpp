#include "drive.h"

Drive::Drive()
{
    // Bits 5-7 are always 1
    hintsts = 0b11100000;
}

bool Drive::clock()
{
    // Check for pending interrupt
    if ((hintmsk & hintsts) != 0)
        return true;

    if (commands_fifo.is_empty())
        return false;

    // Interrupts are queued; don't raise until the last one was acknowledged
    // if ((hintsts & 0b11111) != 0)
    //     return false;

    const u8 command = commands_fifo.pop();

    const auto int3 = [&]() { hintsts = (hintsts & 0b11111000) | (u8)InterruptType::Acknowledge; };
    const auto int5 = [&]() { hintsts = (hintsts & 0b11111000) | (u8)InterruptType::DiskError; };
    const auto stat = [&]() -> u8
    {
        // Motor on; bit 4 is shell open
        return 0b10 | ((u8)shell_open << 4);
    };

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
            results_fifo.push(stat());
            shell_open = false;
            int3();
            break;

        case 0x19:
        {
            // Test / "sub_function"
            const u8 sub_function = parameters_fifo.pop();
            if (sub_function != 0x20) dbg("TODO: unknown CDROM sub-function");

            // 0x20: Get CDROM BIOS date / version
            results_fifo.push(94);  // Year
            results_fifo.push(12);  // Month
            results_fifo.push(3);   // Day
            results_fifo.push(1);   // Version
            int3();
            break;
        }

        case 0x1A:
        {
            // GetID
            if (!get_id_was_called)
            {
                // Signal that we're licensed
                results_fifo.push(stat());
                int3();
            }
            else
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
                int5();
            }
            get_id_was_called = !get_id_was_called;
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
