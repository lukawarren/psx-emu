#pragma once

enum class Interrupt
{
    VBlank = 0,
    GPU = 1,
    CDROM = 2,
    DMA = 3,
    Timer0 = 4,
    Timer1 = 5,
    Timer2 = 6,
    ControllerAndMemoryCard = 7,
    SIO = 8,
    SPU = 9,
    ControllerLightPen = 10
};
