#ifndef HAL_QEMU_DEFAULT_INIT_EVENT_INFRASTRUCTURE_HPP
#define HAL_QEMU_DEFAULT_INIT_EVENT_INFRASTRUCTURE_HPP

#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/qemu/cortex/EventDispatcherCortex.hpp"
#include "hal/qemu/cortex/SystemTickTimerService.hpp"
#include "infra/timer/TimerService.hpp"
#include <chrono>
#include <cstdint>

namespace main_
{
    struct EventInfrastructure
    {
        explicit EventInfrastructure(uint32_t coreClockHz = 25000000,
            infra::Duration tickDuration = std::chrono::milliseconds(1));

        void Run();

        // Declared first so that it outlives every InterruptHandler registered into it.
        hal::cortex::InterruptTable::WithStorage<64> interruptTable;
        hal::cortex::EventDispatcherCortex::WithSize<50> eventDispatcher;
        hal::cortex::SystemTickTimerService systemTick;
    };
}

#endif
