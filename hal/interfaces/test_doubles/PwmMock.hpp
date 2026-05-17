#ifndef HAL_PWM_MOCK_HPP
#define HAL_PWM_MOCK_HPP

#include "hal/interfaces/Pwm.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class PwmMock
        : public SingleChannelPwm
    {
    public:
        MOCK_METHOD(void, SetBaseFrequency, (Hertz baseFrequency), (override));
        MOCK_METHOD(void, Start, (Percent globalDutyCycle), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };
}

#endif
