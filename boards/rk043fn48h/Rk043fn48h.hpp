#ifndef BOARDS_RK043FN48H_RK043FN48H_HPP
#define BOARDS_RK043FN48H_RK043FN48H_HPP

#include "hal/interfaces/DisplayTiming.hpp"

namespace boards
{
    inline constexpr hal::DisplayTiming rk043fn48hTiming{ 9'600'000, { 480, 272 }, 32, 41, 2, 2, 10, 2 };
}

#endif
