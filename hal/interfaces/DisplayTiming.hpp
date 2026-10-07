#ifndef HAL_DISPLAY_TIMING_HPP
#define HAL_DISPLAY_TIMING_HPP

#include "hal/interfaces/Display.hpp"
#include <cstdint>

namespace hal
{
    struct DisplayTiming
    {
        uint32_t pixelClockHz;
        DisplaySize active;
        uint16_t horizontalFrontPorch;
        uint16_t horizontalSync;
        uint16_t horizontalBackPorch;
        uint16_t verticalFrontPorch;
        uint16_t verticalSync;
        uint16_t verticalBackPorch;
        bool hsyncActiveHigh{ false };
        bool vsyncActiveHigh{ false };
        bool dataEnableActiveHigh{ false };
        bool pixelClockInverted{ false };
    };
}

#endif
