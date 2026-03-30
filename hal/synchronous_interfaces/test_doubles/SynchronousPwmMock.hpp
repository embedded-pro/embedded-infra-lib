#ifndef HAL_PWM_MOCK_HPP
#define HAL_PWM_MOCK_HPP

#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class SynchronousPwmMock
        : public SynchronousPwm
    {
    public:
        MOCK_METHOD(void, SetBaseFrequency, (Hertz baseFrequency));
        MOCK_METHOD(void, Start, (Percent globalDutyCycle));
        MOCK_METHOD(void, Stop, ());
    };
}

#endif
