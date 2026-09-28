#ifndef HAL_SYNCHRONOUS_PWM_MOCK_HPP
#define HAL_SYNCHRONOUS_PWM_MOCK_HPP

#include "hal/synchronous_interfaces/SynchronousPwm.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class SynchronousPwmMock
        : public SynchronousSingleChannelPwm
    {
    public:
        MOCK_METHOD(void, SetBaseFrequency, (Hertz baseFrequency), (override));
        MOCK_METHOD(void, Start, (DutyCycle globalDutyCycle), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };
}

#endif
