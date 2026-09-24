#ifndef HAL_PWM_HPP
#define HAL_PWM_HPP

#include "hal/interfaces/DutyCycle.hpp"
#include "infra/util/Unit.hpp"

namespace hal
{
    using Hertz = infra::Quantity<infra::Hertz, uint32_t>;

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
        virtual void Start(DutyCycle globalDutyCycle) = 0;
    };

    class TwoChannelsPwm
        : public Pwm
    {
    public:
        virtual void Start(DutyCycle dutyCycle1, DutyCycle dutyCycle2) = 0;
    };

    class ThreeChannelsPwm
        : public Pwm
    {
    public:
        virtual void Start(DutyCycle dutyCycle1, DutyCycle dutyCycle2, DutyCycle dutyCycle3) = 0;
    };

    class FourChannelsPwm
        : public Pwm
    {
    public:
        virtual void Start(DutyCycle dutyCycle1, DutyCycle dutyCycle2, DutyCycle dutyCycle3, DutyCycle dutyCycle4) = 0;
    };
}

#endif
