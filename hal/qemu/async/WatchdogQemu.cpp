#include "hal/qemu/async/WatchdogQemu.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <limits>

namespace
{
    uint32_t ToTicks(uint32_t clockHz, infra::Duration duration)
    {
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
        really_assert(microseconds > 0);
        auto ticks = (static_cast<uint64_t>(clockHz) * static_cast<uint64_t>(microseconds)) / 1000000u;
        really_assert(ticks > 0 && ticks <= std::numeric_limits<uint32_t>::max());
        return static_cast<uint32_t>(ticks);
    }
}

namespace hal
{
    WatchdogQemu::WatchdogQemu()
        : WatchdogQemu(Config())
    {}

    WatchdogQemu::WatchdogQemu(const Config& config)
        : base(config.base)
        , timeout(config.timeout)
        , resetOnMissedInterrupt(config.resetOnMissedInterrupt)
    {
        auto& watchdog = Peripheral();

        Unlock();

        watchdog.control = 0;
        watchdog.intClr = 0;
        watchdog.load = ToTicks(config.clockHz, config.timeout);

        // The watchdog drives NMI on the MPS2 machines, which is always enabled and has a fixed priority,
        // so the handler has to be in place before the peripheral is allowed to raise it
        Register(cortex::nmiIrq);
    }

    WatchdogQemu::~WatchdogQemu()
    {
        auto& watchdog = Peripheral();
        Unlock();
        watchdog.control = 0;
        watchdog.intClr = 0;
    }

    void WatchdogQemu::Refresh()
    {
        Peripheral().intClr = 0;
    }

    infra::Duration WatchdogQemu::EarlyWarningPeriod() const
    {
        return timeout;
    }

    void WatchdogQemu::Start(const infra::Function<void()>& onEarlyWarning)
    {
        this->onEarlyWarning = onEarlyWarning;

        Refresh();
        Peripheral().control = watchdogControlIntEnable | (resetOnMissedInterrupt ? watchdogControlResetEnable : 0);
    }

    void WatchdogQemu::Invoke()
    {
        if ((Peripheral().ris & watchdogRisTimeout) == 0)
            return;

        onEarlyWarning();
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
}
