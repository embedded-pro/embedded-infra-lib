#ifndef BOARDS_RK043FN48H_RK043FN48H_HPP
#define BOARDS_RK043FN48H_RK043FN48H_HPP

#include "hal/interfaces/DisplayTiming.hpp"

namespace boards
{
    // Rocktech RK043FN48H-CT672B, a 4.3" 480 x 272 TFT with a 24-bit RGB interface that is driven in data enable mode and has no controller to initialise.
    // The timing is the one of ST's board support package for the STM32H745I-DISCO: the data starts 43 clocks after the start of HSYNC and 12 lines after the start of VSYNC.
    inline constexpr hal::DisplayTiming rk043fn48hTiming{ 9'600'000, { 480, 272 }, 32, 41, 2, 2, 10, 2 };
}

#endif
