#ifndef HAL_CORTEX_M_LOW_POWER_MODE_CORTEX_HPP
#define HAL_CORTEX_M_LOW_POWER_MODE_CORTEX_HPP

#include "hal/interfaces/LowPowerMode.hpp"

namespace hal::cortex
{
    // Deep sleep needs vendor-specific clock configuration, so without it the core only sleeps
    class LowPowerModeCortex
        : public LowPowerMode
    {
    public:
        void Enter(PowerMode mode) override;
    };

    void WaitForInterrupt(bool deepSleep);
}

#endif
