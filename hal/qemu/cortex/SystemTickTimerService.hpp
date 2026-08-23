#ifndef HAL_QEMU_CORTEX_SYSTEM_TICK_TIMER_SERVICE_HPP
#define HAL_QEMU_CORTEX_SYSTEM_TICK_TIMER_SERVICE_HPP

#include "hal/qemu/cortex/InterruptCortex.hpp"
#include "hal/qemu/cortex/SystemTick.hpp"
#include "infra/timer/TickOnInterruptTimerService.hpp"
#include "infra/util/InterfaceConnector.hpp"

namespace hal::cortex
{
    class SystemTickTimerService
        : public infra::InterfaceConnector<SystemTickTimerService>
        , public infra::TickOnInterruptTimerService
        , private InterruptHandler
    {
    public:
        explicit SystemTickTimerService(uint32_t coreClockHz,
            infra::Duration tickDuration = std::chrono::milliseconds(1),
            uint32_t id = infra::systemTimerServiceId);

    private:
        void Invoke() override;

    private:
        SystemTick systemTick;
    };
}

#endif
