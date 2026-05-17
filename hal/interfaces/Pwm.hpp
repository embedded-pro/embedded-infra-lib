#ifndef HAL_PWM_HPP
#define HAL_PWM_HPP

#include "infra/util/Unit.hpp"

namespace hal
{
    using Hertz = infra::Quantity<infra::Hertz, uint32_t>;
    using Percent = infra::Quantity<infra::Percent, uint8_t>;

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
        virtual void Start(Percent globalDutyCycle) = 0;
    };

    class TwoChannelsPwm
        : public Pwm
    {
    public:
        virtual void Start(Percent dutyCycle1, Percent dutyCycle2) = 0;
    };

    class ThreeChannelsPwm
        : public Pwm
    {
    public:
        virtual void Start(Percent dutyCycle1, Percent dutyCycle2, Percent dutyCycle3) = 0;
    };

    class FourChannelsPwm
        : public Pwm
    {
    public:
        virtual void Start(Percent dutyCycle1, Percent dutyCycle2, Percent dutyCycle3, Percent dutyCycle4) = 0;
    };
}

#endif
