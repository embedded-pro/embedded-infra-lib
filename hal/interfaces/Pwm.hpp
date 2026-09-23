#ifndef HAL_PWM_HPP
#define HAL_PWM_HPP

#include "infra/util/Unit.hpp"

namespace hal
{
    using Hertz = infra::Quantity<infra::Hertz, uint32_t>;
    using Percent = infra::Quantity<infra::Percent, uint8_t>;
    using FractionalPercent = infra::Quantity<infra::Percent, float>;

    class Pwm
    {
    public:
        virtual void SetBaseFrequency(Hertz baseFrequency) = 0;
        virtual void Stop() = 0;
    };

    class SingleChannelPwm
        : public Pwm
    {
    public:
        virtual void Start(FractionalPercent globalDutyCycle) = 0;
    };

    class TwoChannelsPwm
        : public Pwm
    {
    public:
        virtual void Start(FractionalPercent dutyCycle1, FractionalPercent dutyCycle2) = 0;
    };

    class ThreeChannelsPwm
        : public Pwm
    {
    public:
        virtual void Start(FractionalPercent dutyCycle1, FractionalPercent dutyCycle2, FractionalPercent dutyCycle3) = 0;
    };

    class FourChannelsPwm
        : public Pwm
    {
    public:
        virtual void Start(FractionalPercent dutyCycle1, FractionalPercent dutyCycle2, FractionalPercent dutyCycle3, FractionalPercent dutyCycle4) = 0;
    };
}

#endif
