#ifndef HAL_GPIO_PIN_NULL_HPP
#define HAL_GPIO_PIN_NULL_HPP

#include "hal/interfaces/Gpio.hpp"

namespace hal
{
    class GpioPinNull
        : public GpioPin
    {
    public:
        bool Get() const override;
        void Set(bool value) override;
        bool GetOutputLatch() const override;
        void SetAsInput() override;
        bool IsInput() const override;
        void Config(PinConfigType config) override;
        void Config(PinConfigType config, bool startOutputState) override;
        void ResetConfig() override;
        void EnableInterrupt(const infra::Function<void()>& action, InterruptTrigger trigger, InterruptType type) override;
        void DisableInterrupt() override;
    };
}

#endif
