#include "hal/qemu/default_init/EventInfrastructure.hpp"

namespace bringup
{
    EventInfrastructure::EventInfrastructure(uint32_t coreClockHz, infra::Duration tickDuration)
        : systemTick(coreClockHz, tickDuration)
    {
        systemTick.Start();
    }

    void EventInfrastructure::Run()
    {
        eventDispatcher.Run();
    }
}
