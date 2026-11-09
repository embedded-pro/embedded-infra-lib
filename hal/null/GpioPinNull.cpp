#include "hal/null/GpioPinNull.hpp"

namespace hal
{
    bool GpioPinNull::Get() const
    {
        return false;
    }

    void GpioPinNull::Set(bool)
    {}

    bool GpioPinNull::GetOutputLatch() const
    {
        return false;
    }

    void GpioPinNull::SetAsInput()
    {}

    bool GpioPinNull::IsInput() const
    {
        return false;
    }

    void GpioPinNull::Config(PinConfigType)
    {}

    void GpioPinNull::Config(PinConfigType, bool)
    {}

    void GpioPinNull::ResetConfig()
    {}

    void GpioPinNull::EnableInterrupt(const infra::Function<void()>&, InterruptTrigger, InterruptType)
    {}

    void GpioPinNull::DisableInterrupt()
    {}
}
