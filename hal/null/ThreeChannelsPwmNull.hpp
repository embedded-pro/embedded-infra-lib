#ifndef HAL_THREE_CHANNELS_PWM_NULL_HPP
#define HAL_THREE_CHANNELS_PWM_NULL_HPP

#include "hal/interfaces/Pwm.hpp"

namespace hal
{
    class ThreeChannelsPwmNull
        : public ThreeChannelsPwm
    {
    public:
        void SetBaseFrequency(Hertz baseFrequency) override;
        void Stop() override;
        void Start(DutyCycle dutyCycle1, DutyCycle dutyCycle2, DutyCycle dutyCycle3) override;
    };
}

#endif
