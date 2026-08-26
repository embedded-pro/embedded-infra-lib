#ifndef HAL_CORTEX_M_SYSTEM_TICK_HPP
#define HAL_CORTEX_M_SYSTEM_TICK_HPP

#include "infra/timer/TimerService.hpp"
#include <cstdint>

namespace hal::cortex
{
    class SystemTick
    {
    public:
        SystemTick(uint32_t coreClockHz, infra::Duration tickDuration);

        void Enable();
        void Disable();

    private:
        uint32_t reload;
    };
}

#endif
