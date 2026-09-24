#ifndef HAL_SYNCHRONOUS_PWM_HPP
#define HAL_SYNCHRONOUS_PWM_HPP

#include "hal/interfaces/DutyCycle.hpp"
#include "infra/util/Unit.hpp"

namespace hal
{
    using Hertz = infra::Quantity<infra::Hertz, uint32_t>;

    class SynchronousPwm
    {
    public:
        virtual void SetBaseFrequency(Hertz baseFrequency) = 0;
        virtual void Stop() = 0;
    };

    class SynchronousSingleChannelPwm
        : public SynchronousPwm
    {
    public:
        virtual void Start(DutyCycle globalDutyCycle) = 0;
    };

    class SynchronousTwoChannelsPwm
        : public SynchronousPwm
    {
    public:
        virtual void Start(DutyCycle dutyCycle1, DutyCycle dutyCycle2) = 0;
    };

    class SynchronousThreeChannelsPwm
        : public SynchronousPwm
    {
    public:
        virtual void Start(DutyCycle dutyCycle1, DutyCycle dutyCycle2, DutyCycle dutyCycle3) = 0;
    };

    class SynchronousFourChannelsPwm
        : public SynchronousPwm
    {
    public:
        virtual void Start(DutyCycle dutyCycle1, DutyCycle dutyCycle2, DutyCycle dutyCycle3, DutyCycle dutyCycle4) = 0;
    };
}

#endif
