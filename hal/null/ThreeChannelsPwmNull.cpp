#include "hal/null/ThreeChannelsPwmNull.hpp"

namespace hal
{
    void ThreeChannelsPwmNull::SetBaseFrequency(Hertz)
    {}

    void ThreeChannelsPwmNull::Stop()
    {}

    void ThreeChannelsPwmNull::Start(DutyCycle, DutyCycle, DutyCycle)
    {}
}
