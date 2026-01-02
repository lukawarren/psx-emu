#include "drive.h"

Drive::Drive()
{
    // Bits 5-7 are always 1
    hintsts = 0b11100000;
}

void Drive::clock()
{
    if ((hintmsk & hintsts) != 0)
    {
        dbg("TODO: interrupt", dbg::bin(hintmsk), dbg::bin(hintsts));
        exit(1);
    }
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
                0b1000 | // Parameter FIFO empty
                0b100000; // Parameter FIFO not full

        case 1:
            dbg("RESULT read");
            break;

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
                    dbg("COMMAND write");
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
                    dbg("PARAMETER write");
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
                    if ((value >> 6) & 0b1) dbg("TODO: clear parameter FIFO");
                    if ((value >> 7) & 0b1) dbg("TODO: reset decoder chip");
                    dbg("TODO: if acknowledge, drain result FIFO and, if there's a "
                        "pending command, send it to the controller");
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
