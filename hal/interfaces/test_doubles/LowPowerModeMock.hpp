#ifndef HAL_LOW_POWER_MODE_MOCK_HPP
#define HAL_LOW_POWER_MODE_MOCK_HPP

#include "hal/interfaces/LowPowerMode.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class LowPowerModeMock
        : public LowPowerMode
    {
    public:
        MOCK_METHOD(void, Enter, (PowerMode mode), (override));
    };
}

#endif
