#ifndef HAL_LOW_POWER_MODE_HPP
#define HAL_LOW_POWER_MODE_HPP

#include <cstdint>

namespace hal
{
    enum class PowerMode : uint8_t
    {
        sleep,
        deepSleep
    };

    class LowPowerMode
    {
    protected:
        LowPowerMode() = default;
        LowPowerMode(const LowPowerMode& other) = delete;
        LowPowerMode& operator=(const LowPowerMode& other) = delete;
        ~LowPowerMode() = default;

    public:
        // Called with interrupts masked. Returns once an interrupt is pending, with the run-mode clocks restored
        virtual void Enter(PowerMode mode) = 0;
    };
}

#endif
