#ifndef HAL_QEMU_DEFAULT_INIT_EVENT_INFRASTRUCTURE_HPP
#define HAL_QEMU_DEFAULT_INIT_EVENT_INFRASTRUCTURE_HPP

#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/EventDispatcherCortex.hpp"
#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "infra/timer/TimerService.hpp"
#include <chrono>
#include <cstdint>

namespace bringup
{
    struct EventInfrastructure
    {
        explicit EventInfrastructure(uint32_t coreClockHz = 25000000,
            infra::Duration tickDuration = std::chrono::milliseconds(1));

        void Run();

        hal::cortex::InterruptTable::WithStorage<64> interruptTable;
        hal::cortex::EventDispatcherCortex::WithSize<50> eventDispatcher;
        hal::cortex::SystemTickTimerService systemTick;
    };
}

#endif
