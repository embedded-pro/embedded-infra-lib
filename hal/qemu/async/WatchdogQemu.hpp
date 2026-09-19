#ifndef HAL_QEMU_ASYNC_WATCHDOG_QEMU_HPP
#define HAL_QEMU_ASYNC_WATCHDOG_QEMU_HPP

#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/qemu/async/CmsdkWatchdogRegisters.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>

namespace hal
{
    class WatchdogQemu
        : private cortex::InterruptHandler
    {
    public:
        struct Config
        {
            constexpr Config()
            {}

            infra::Duration timeout{ std::chrono::milliseconds(50) };
            infra::Duration feedTimerInterval{ std::chrono::milliseconds(25) };
            infra::Duration expirationTimeout{ std::chrono::milliseconds(1500) };
            bool resetOnMissedInterrupt{ true };
            uintptr_t base{ cmsdkWatchdogBaseAddress };
            uint32_t clockHz{ cmsdkWatchdogClockHz };
        };

        WatchdogQemu(const infra::Function<void()>& onExpired, const Config& config = Config());
        ~WatchdogQemu();

        void Refresh();

    private:
        void Invoke() override;
        CmsdkWatchdogRegisters& Peripheral() const;
        void Unlock() const;
        void Feed();

        uintptr_t base;
        uint32_t expirationCount{ 1 };
        std::atomic<uint32_t> missedFeeds{ 0 };
        infra::Function<void()> onExpired;
        infra::TimerRepeating feedTimer;
    };
}

#endif
