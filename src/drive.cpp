#include "drive.h"

constexpr static u32 cdrom_command_delay = 0x1100;
constexpr static u32 cdrom_read_play_delay = 0x6e400;
constexpr static u32 sector_size = 0x930;

Drive::Drive() : cue(
    "/home/luka/src/luka/psx-emu/ROMs/Crash Bandicoot (USA).cue",
    "/home/luka/src/luka/psx-emu/ROMs/Crash Bandicoot (USA).bin"
)
{
}

bool Drive::clock()
{
    if ((interrupt_status & 0b111) == 0 && status.shell_open)
    {
        // Can't execute commands when the shell is open
        push_status();
        raise_interrupt(InterruptType::DiskError);
        return (interrupt_mask & interrupt_status) != 0;
    }

    if (command_delay_timer > 0)
        command_delay_timer--;

    // Handle command if delay is up and no interrupts due
    if (command_delay_timer == 0 && (interrupt_status & 0b111) == 0)
    {
        if (command_state == CommandState::None)
        {
            if (command.has_value())
            {
                on_command(*command);
                command.reset();
            }
        }
        else
            continue_previous_command();

    }

    if (status.mode == Status::Mode::Read)
    {
        if (read_play_delay_timer > 0)
            read_play_delay_timer--;

        // Again, only if no interrupts due
        if (read_play_delay_timer == 0 && (interrupt_status & 0b111) == 0)
            on_read();
    }

    // Raise interrupt if need be
    return (interrupt_mask & interrupt_status) != 0;
}

u8 Drive::read(const u32 address)
{
    switch (address)
    {
        case 0:
            // HSTS
            return fifo_status.to_u8();

        case 1:
            // RESULT
            return pop_result();

        case 2:
            // RDDATA
            return pop_data();

        case 3:
            // HINTMSK; upper 3 bits always one when read (reserved)
            if (fifo_status.bank == 0 || fifo_status.bank == 2)
                return (interrupt_mask & 0b11111) | 0b11100000;

            // HINTSTS; as above
            else return (interrupt_status & 0b11111) | 0b11100000;
    }

    return 0;
}

void Drive::write(const u32 address, const u8 value)
{
    switch (address)
    {
        case 0:
            // ADDRESS; read-only save for bank field
            fifo_status.bank = value & 0b11;
            break;

        case 1:
            switch (fifo_status.bank)
            {
                case 0:
                    push_command(value);
                    break;

                case 1:
                    dbg("TODO: WRDATA write");
                    exit(1);
                    break;

                case 2:
                    dbg("TODO: CI write");
                    exit(1);
                    break;

                case 3:
                    dbg("TODO: ATV2 write");
                    exit(1);
                    break;
            }
            break;

        case 2:
            switch (fifo_status.bank)
            {
                case 0:
                    // PARAMETER
                    push_parameter(value);
                    break;

                case 1:
                    // HINTMSK; bits 5-7 reserved
                    interrupt_mask = value & 0b11111;
                    break;

                case 2:
                    dbg("TODO: AVT0 write");
                    exit(1);
                    break;

                case 3:
                    dbg("TODO: ATV3 write");
                    exit(1);
                    break;
            }
            break;

        case 3:
            switch (fifo_status.bank)
            {
                case 0:
                    dbg("TODO: HCHPCTL write");
                    break;

                case 1:
                    // HCLRCTL write; writing to botttom 5 bits ACK's interrupt
                    interrupt_status &= ~(value & 0b11111);

                    if ((value & 0b100000) != 0) dbg("TODO: clear sound map XA-ADPCM buffer");
                    if ((value & 0b1000000) != 0) reset_parameter_fifo();
                    if ((value & 0b10000000) != 0) dbg("TODO: clear sound map XA-ADPCM buffer");

                    break;

                case 2:
                    dbg("TODO: AVT1 write");
                    exit(1);
                    break;

                case 3:
                    dbg("TODO: ADPCTL write");
                    exit(1);
                    break;
            }
            break;
    }
}

void Drive::continue_previous_command()
{
    command_delay_timer = cdrom_command_delay;

    switch (command_state)
    {
        case CommandState::GetID:
            // Force licensed response; NTSC
            push_result(0x02);
            push_result(0x00);
            push_result(0x20);
            push_result(0x00);
            push_result('S');
            push_result('C');
            push_result('E');
            push_result('A');
            raise_interrupt(InterruptType::Complete);
        break;

        case CommandState::SeekL:
            push_status();
            raise_interrupt(InterruptType::Complete);
        break;

        case CommandState::Pause:
            push_status();
            raise_interrupt(InterruptType::Complete);
        break;

        default: break;
    }
    command_state = CommandState::None;
}

void Drive::on_command(const u8 opcode)
{
    command_delay_timer = cdrom_command_delay;

    // All commands start the motor
    status.motor_on = true;

    const auto int3_stat = [&]() { push_status(); raise_interrupt(InterruptType::Acknowledge); };

    switch (opcode)
    {
        case 0x01:
        {
            // NOP
            push_status();
            raise_interrupt(status.error ?
                InterruptType::DiskError :
                InterruptType::Acknowledge
            );
            status.error = false;
        }
        break;

        case 0x02:
            // Setloc
            set_loc_data.minutes = from_bcd(pop_parameter());
            set_loc_data.seconds = from_bcd(pop_parameter());
            set_loc_data.sector = from_bcd(pop_parameter());
            int3_stat();
        break;

        case 0x06:
            // ReadN
            on_seek();
            status.mode = Status::Mode::Read;
            int3_stat();

            read_play_delay_timer = mode.double_speed ?
                cdrom_read_play_delay / 2 :
                cdrom_read_play_delay;

            read_buffer.clear();
        break;

        case 0x09:
            // Pause
            status.mode = Status::Mode::None;
            int3_stat();
            command_state = CommandState::Pause;
        break;

        case 0x0E:
            // Setmode
            mode.from_u8(pop_parameter());
            int3_stat();
        break;

        case 0x15:
            // SeekL
            on_seek();
            int3_stat();
            command_state = CommandState::SeekL;
        break;

        case 0x19:
        {
            // TEST
            const u8 sub_function = pop_parameter();
            if (sub_function != 0x20)
            {
                dbg("TODO: CDROM TEST sub_function", dbg::hex(sub_function));
                exit(1);
            }
            else
            {
                push_result(99);    // Year
                push_result(2);     // Month
                push_result(1);     // Day
                push_result(0xc3);  // Version
                raise_interrupt(InterruptType::Acknowledge);
            }
        }
        break;

        case 0x1A:
            // GetID
            int3_stat();
            command_state = CommandState::GetID;
        break;

        default:
            dbg("TODO: unknown CDROM command", dbg::hex(opcode));
            exit(1);
    }
}

void Drive::reset_parameter_fifo()
{
    fifo_status.parameter_fifo_empty = true;
    fifo_status.parameter_fifo_not_full = true;
    parameter_fifo.clear();
}

void Drive::push_parameter(const u8 value)
{
    parameter_fifo.push(value);
    fifo_status.parameter_fifo_empty = false;
    if (parameter_fifo.is_full())
        fifo_status.parameter_fifo_not_full = false;
}

void Drive::push_command(const u8 value)
{
    command = { value };
    command_delay_timer = cdrom_command_delay;
    command_state = CommandState::None;
}

void Drive::push_result(const u8 value)
{
    result_fifo.push(value);
    fifo_status.result_data_ready = true;
}

void Drive::push_status()
{
    push_result(status.to_u8());
}

u8 Drive::pop_parameter()
{
    const u8 value = parameter_fifo.pop();
    fifo_status.parameter_fifo_empty = parameter_fifo.is_empty();
    fifo_status.parameter_fifo_not_full = false;
    return value;
}

u8 Drive::pop_result()
{
    const u8 value = result_fifo.pop();
    fifo_status.result_data_ready = !result_fifo.is_empty();
    return value;
}

u8 Drive::pop_data()
{
    // TODO: should actually pad to last X bytes??
    if (read_buffer_index >= read_buffer_length || read_buffer_index >= read_buffer.size())
        return 0;

    const u8 data = read_buffer[read_buffer_index++];

    if (read_buffer_index == read_buffer_length)
    {
        read_buffer.clear();
        read_buffer_index = 0;
        fifo_status.data_request = false;
    }

    return data;
}

void Drive::on_seek()
{
    status.mode = Status::Mode::Seek;

    const u32 minutes = set_loc_data.minutes;
    const u32 seconds = set_loc_data.seconds;
    const u32 sector = set_loc_data.sector;

    dbg("TODO: figure out where's best to minus 2");
    seek_sector = ((minutes * 60 + seconds - 2) * 75 + sector);
}

void Drive::on_read()
{
    const u32 sector_start = seek_sector * sector_size;

    // Read data; skip 12 sync bytes and optionally more
    const u8* const data = cue.read_sector(sector_start)
        + 12
        + (mode.sector_size ? 0 : 12);
    const u32 length = sector_size - 12 - (mode.sector_size ? 0 : 12);

    read_buffer.clear();
    read_buffer_length = length;

    for (u32 i = 0; i < length; i++)
        read_buffer.push_back(data[i]);

    fifo_status.result_data_ready = true;

    push_status();
    raise_interrupt(InterruptType::DataReady);
}

void Drive::raise_interrupt(const InterruptType interrupt)
{
    interrupt_status &= (u8)~(0b111);
    interrupt_status |= ((u8)interrupt & 0b111);
}