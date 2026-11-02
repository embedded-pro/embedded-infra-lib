#ifndef HAL_CORTEX_M_JUMP_TO_APPLICATION_HPP
#define HAL_CORTEX_M_JUMP_TO_APPLICATION_HPP

#include <cstdint>

namespace hal::cortex
{
    [[noreturn]] void JumpToApplication(uint32_t vectorTableAddress);
}

#endif
