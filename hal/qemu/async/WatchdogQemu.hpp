#ifndef HAL_QEMU_ASYNC_WATCHDOG_QEMU_HPP
#define HAL_QEMU_ASYNC_WATCHDOG_QEMU_HPP

#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/interfaces/Watchdog.hpp"
#include "hal/qemu/async/CmsdkWatchdogRegisters.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>

namespace hal
{
    class WatchdogQemu
        : public WatchdogWithEarlyWarning
        , private cortex::InterruptHandler
    {
    public:
        struct Config
        {
            constexpr Config()
            {}

            infra::Duration timeout{ std::chrono::milliseconds(50) };
            bool resetOnMissedInterrupt{ true };
            uintptr_t base{ cmsdkWatchdogBaseAddress };
            uint32_t clockHz{ cmsdkWatchdogClockHz };
        };

        explicit WatchdogQemu(const Config& config = Config());
        ~WatchdogQemu();

        void Refresh() override;
        infra::Duration EarlyWarningPeriod() const override;
        void Start(const infra::Function<void()>& onEarlyWarning) override;

    private:
        void Invoke() override;
        CmsdkWatchdogRegisters& Peripheral() const;
        void Unlock() const;

        uintptr_t base;
        infra::Duration timeout;
        bool resetOnMissedInterrupt;
        infra::Function<void()> onEarlyWarning;
    };
}

#endif
