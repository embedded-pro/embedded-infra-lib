#include "hal/qemu/async/WatchdogQemu.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>
#include <limits>

namespace
{
    uint64_t ToMicroseconds(infra::Duration duration)
    {
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        really_assert(microseconds > 0);
        return static_cast<uint64_t>(microseconds);
    }

    uint32_t ToTicks(uint32_t clockHz, infra::Duration duration)
    {
        auto ticks = (static_cast<uint64_t>(clockHz) * ToMicroseconds(duration)) / 1000000u;
        really_assert(ticks > 0 && ticks <= std::numeric_limits<uint32_t>::max());
        return static_cast<uint32_t>(ticks);
    }

    uint32_t ToNumberOfTimeouts(infra::Duration expirationTimeout, infra::Duration timeout)
    {
        auto timeoutMicroseconds = ToMicroseconds(timeout);
        auto count = (ToMicroseconds(expirationTimeout) + timeoutMicroseconds - 1) / timeoutMicroseconds;
        return static_cast<uint32_t>(std::max<uint64_t>(count, 1));
    }
}

namespace hal
{
    WatchdogQemu::WatchdogQemu(const infra::Function<void()>& onExpired, const Config& config)
        : base(config.base)
        , expirationCount(ToNumberOfTimeouts(config.expirationTimeout, config.timeout))
        , onExpired(onExpired)
    {
        really_assert(config.feedTimerInterval > infra::Duration::zero());

        auto& watchdog = Peripheral();

        Unlock();

        watchdog.control = 0;
        watchdog.intClr = 0;
        watchdog.load = ToTicks(config.clockHz, config.timeout);

        // The watchdog drives NMI on the MPS2 machines, which is always enabled and has a fixed priority,
        // so the handler has to be in place before the peripheral is allowed to raise it
        Register(cortex::nmiIrq);

        watchdog.control = watchdogControlIntEnable | (config.resetOnMissedInterrupt ? watchdogControlResetEnable : 0);

        feedTimer.Start(config.feedTimerInterval, [this]()
            {
                Feed();
            });
    }

    WatchdogQemu::~WatchdogQemu()
    {
        feedTimer.Cancel();

        auto& watchdog = Peripheral();
        Unlock();
        watchdog.control = 0;
        watchdog.intClr = 0;
    }

    void WatchdogQemu::Refresh()
    {
        Peripheral().intClr = 0;
    }

    void WatchdogQemu::Invoke()
    {
        if ((Peripheral().ris & watchdogRisTimeout) == 0)
            return;

        Refresh();

        if (++missedFeeds == expirationCount)
            onExpired();
    }

    CmsdkWatchdogRegisters& WatchdogQemu::Peripheral() const
    {
        return *reinterpret_cast<CmsdkWatchdogRegisters*>(base);
    }

    void WatchdogQemu::Unlock() const
    {
        // Registers are left unlocked because the interrupt handler writes intClr on every timeout
        Peripheral().lock = watchdogLockUnlockKey;
    }

    void WatchdogQemu::Feed()
    {
        missedFeeds = 0;
    }
}
