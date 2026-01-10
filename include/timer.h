#pragma once

template<int i>
struct TimerCounterMode
{
    enum class SynchronisationMode
    {
        // Counter 0
        PauseCounterDuringHBlanks = 0,
        ResetCounterToZeroAtHBlanks = 1,
        ResetCounterToZeroAtHBlanksAndPauseOutsideHBlanks = 2,
        PauseUntilHBlankThenSwitchToFreeRun = 3,

        // Counter 1
        PauseCounterDuringVBlanks = 4,
        ResetCounterToZeroAtVBlanks = 5,
        ResetCounterToZeroAtVBlanksAndPauseOutsideVBlanks = 6,
        PauseUntilVBlankThenSwitchToFreeRun = 7,

        // Counter 2
        StopCounterAtCurrentValue = 8,
        FreeRun = 9
    };

    enum class ClockSource
    {
        SystemClock,
        DotClock,
        HBlank,
        SystemClockOverEight
    };

    u8 raw_synchronisation_mode;
    u8 raw_clock_source;

    void write(const u16 value)
    {
        raw_synchronisation_mode = (value >> 1) & 0b11;
        raw_clock_source = (value >> 8) & 0b11;

        synchronisation_enable = value & 0b1;

        if constexpr (i == 0)
            synchronisation_mode = (SynchronisationMode)((value >> 1) & 0b11);

        if constexpr (i == 1)
            synchronisation_mode = (SynchronisationMode)(((value >> 1) & 0b11) + 4);

        if constexpr (i == 2)
        {
            synchronisation_mode =
                raw_synchronisation_mode == 0 || raw_synchronisation_mode == 3 ?
                SynchronisationMode::StopCounterAtCurrentValue :
                SynchronisationMode::FreeRun;
        }

        reset_counter_after_target = (value >> 3) & 0b1;
        irq_on_target_reached = (value >> 4) & 0b1;
        irq_on_max_value_reached = (value >> 5) & 0b1;
        fire_irq_repeatedly = (value >> 6) & 0b1;
        irq_toggle_mode = (value >> 7) & 0b1;

        if constexpr (i == 0)
        {
            if (raw_clock_source == 0 || raw_clock_source == 2)
                clock_source = ClockSource::SystemClock;
            else
                clock_source = ClockSource::DotClock;
        }

        if constexpr (i == 1)
        {
            if (raw_clock_source == 0 || raw_clock_source == 2)
                clock_source = ClockSource::SystemClock;
            else
                clock_source = ClockSource::HBlank;
        }

        if constexpr (i == 2)
        {
            if (raw_clock_source == 0 || raw_clock_source == 2)
                clock_source = ClockSource::SystemClock;
            else
                clock_source = ClockSource::SystemClockOverEight;
        }

        // Always set to 1 when writing
        interrupt_request = true;

        // Remaining values are read-only
    }

    u16 read()
    {
        u16 value = 0;

        value |= (u16)synchronisation_enable << 0;
        value |= ((u16)raw_synchronisation_mode & 0b11) << 1;
        value |= (u16)reset_counter_after_target << 3;
        value |= (u16)irq_on_target_reached << 4;
        value |= (u16)irq_on_max_value_reached << 5;
        value |= (u16)fire_irq_repeatedly << 6;
        value |= (u16)irq_toggle_mode << 7;
        value |= ((u16)raw_clock_source & 0b11) << 8;
        value |= (u16)interrupt_request << 10;
        value |= (u16)reached_target_value << 11;
        value |= (u16)reached_max_value << 12;

        // Reset after reading
        reached_target_value = false;
        reached_max_value = false;

        return value;
    }

    bool synchronisation_enable;
    SynchronisationMode synchronisation_mode;
    bool reset_counter_after_target; // as opposed to after 0xFFFF
    bool irq_on_target_reached;
    bool irq_on_max_value_reached;
    bool fire_irq_repeatedly;
    bool irq_toggle_mode; // as opposed to "pulse mode"
    ClockSource clock_source;
    bool interrupt_request;
    bool reached_target_value;
    bool reached_max_value;
};