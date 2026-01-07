#include "drive.h"

Drive::Drive() : cue(
    "/home/luka/src/luka/psx-emu/ROMs/Crash Bandicoot (USA).cue",
    "/home/luka/src/luka/psx-emu/ROMs/Crash Bandicoot (USA).bin"
)
{
    // Bits 5-7 are always 1
    hintsts = 0b11100000;
    second_response = SecondResponse::None;
    read_n_mode = ReadNMode::None;
}

bool Drive::clock()
{
    const auto stat = [&]() -> u8
    {
        // Motor on
        u8 value = 0b10;

        // Lid is or was open
        value |= ((u8)shell_open << 4);

        // Finished seeking (i.e. seek bit off, read bit maybe on)
        value |= ((u8)reading_data_sector << 5);
        return value;
    };
    const auto int1 = [&]() { hintsts = (hintsts & 0b11111000) | (u8)InterruptType::DataReady; };
    const auto int2 = [&]() { hintsts = (hintsts & 0b11111000) | (u8)InterruptType::Complete; };
    const auto int3 = [&]() { hintsts = (hintsts & 0b11111000) | (u8)InterruptType::Acknowledge; };
    const auto int5 = [&]() { hintsts = (hintsts & 0b11111000) | (u8)InterruptType::DiskError; };
    const auto int1_stat = [&]() { results_fifo.push(stat()); int1(); };
    const auto int2_stat = [&]() { results_fifo.push(stat()); int2(); };
    const auto int3_stat = [&]() { results_fifo.push(stat()); int3(); };
    const auto int5_stat = [&]() { results_fifo.push(stat()); int5(); };

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
        int2_stat();
    }
    else if (second_response == SecondResponse::Pause)
    {
        int2_stat();
    }
    else if (second_response == SecondResponse::Init)
    {
        int2_stat();
    }

    if (second_response != SecondResponse::None)
    {
        second_response = SecondResponse::None;
        return false;
    }

    if (commands_fifo.is_empty())
        return false;

    const u8 command = commands_fifo.pop();
dbg(dbg::hex(command));
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
        {
            // SetLoc
            const u8 minute = from_bcd(parameters_fifo.pop());
            const u8 second = from_bcd(parameters_fifo.pop());
            const u8 frame = from_bcd(parameters_fifo.pop());
            seek_target = msf_to_lba(minute, second, frame);
            int3_stat();
            break;
        }

        case 0x06:
            // ReadN
            read_n_mode = ReadNMode::Interrupt;
            reading_data_sector = true;
            int3_stat();
            break;

        case 0x09:
            // Pause
            int3_stat();
            second_response = SecondResponse::Pause;
            read_n_mode = ReadNMode::None;
            data_request_pending = false;
            reading_data_sector = false;
            dbg("TODO: paused; do for ReadS + playing too");
            break;

        case 0x0A:
            // Init
            dbg("TODO: Init() - set mode=0x20, activate drive motor, standby, abort all commands");
            int3_stat();
            second_response = SecondResponse::Init;
            break;

        case 0x0E:
        {
            // Setmode
            const u8 mode = parameters_fifo.pop();
            dbg("TODO: setmode (including sector size)", dbg::bin(mode));
            if ((mode & 0b10) != 0) dbg("TODO: auto-pause");
            if ((mode & 0b10000) != 0) dbg("TODO: ignore bit");
            set_mode_sector_size = (SectorSize)((mode >> 5) & 0b1);
            int3_stat();
            break;
        }

        case 0x13:
            // GetTN
            dbg("TODO: GetTN() - what is TOC?");
            parameters_fifo.push(stat());
            parameters_fifo.push(to_bcd(1)); // First track number
            parameters_fifo.push(to_bcd(1)); // Last track number
            break;

        case 0x15:
            // SeekL
            seek_data = seek_target;
            dbg("stop any reading or pending ReadS too");
            read_n_mode = ReadNMode::None;
            data_request_pending = false;
            reading_data_sector = false;
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

    // Executed command, but could be reading at same time
    if (read_n_mode != ReadNMode::None)
    {
        if (read_n_mode == ReadNMode::Interrupt)
        {
            int1_stat();
            read_n_mode = ReadNMode::Data;
            data_request_pending = true;

            // Fetch actual sector
            read_n_mode = ReadNMode::Data;
            read_n_sector = cue.read_sector(seek_data);
            read_n_byte = 0;
        }
    }

    return false;
}

u8 Drive::read(const u32 address)
{
    dbg("CD read");
    switch (address)
    {
        case 0:
            // HSTS
            dbg("TODO: rest of HSTS bits");
            return
                (bank & 0b11) |
                (parameters_fifo.is_empty() ? 0b1000 : 0) |
                (parameters_fifo.is_full() ? 0 : 0b100000) |
                (results_fifo.is_empty() ? 0 : 0b100000) |
                (data_request_pending ? 0b1000000 : 0);

        case 1:
            // RESULT
            return results_fifo.pop();

        case 2:
        {
            // RDDATA
            u32 data_start = 12;
            if (set_mode_sector_size == SectorSize::DataOnly)
                data_start += 12;

            // If outside region of data buffer, pad
            if (set_mode_sector_size == SectorSize::DataOnly && read_n_byte >= 0x800)
            {
                read_n_byte++;
                return read_n_sector[data_start + 0x800 - 8];
            }
            else if (read_n_byte >= 0x924)
            {
                read_n_byte++;
                return read_n_sector[data_start + 0x924 - 4];
            }

            const u8 data = read_n_sector[data_start + read_n_byte++];

            // If buffer is now empty, set DRQSTS to false
            if ((set_mode_sector_size == SectorSize::DataOnly && read_n_byte >= 0x800) || read_n_byte >= 0x924)
                data_request_pending = false;

            return data;
        }

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

