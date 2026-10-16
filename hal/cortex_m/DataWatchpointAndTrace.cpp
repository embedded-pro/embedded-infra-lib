#include "hal/cortex_m/DataWatchpointAndTrace.hpp"

namespace
{
    constexpr uintptr_t coreDebugDemcr = 0xE000EDFCu;
    constexpr uint32_t demcrTrcena = 1u << 24;

    constexpr uintptr_t dwtCtrl = 0xE0001000u;
    constexpr uint32_t dwtCyccntena = 1u << 0;

    constexpr uintptr_t dwtCyccnt = 0xE0001004u;

    constexpr uintptr_t dwtLar = 0xE0001FB0u;
    constexpr uint32_t larUnlockKey = 0xC5ACCE55u;

    constexpr uintptr_t dwtLsr = 0xE0001FB4u;
    constexpr uint32_t lsrLockImplemented = 1u << 0;
    constexpr uint32_t lsrLocked = 1u << 1;

    volatile uint32_t& Reg32(uintptr_t address)
    {
        return *reinterpret_cast<volatile uint32_t*>(address);
    }

    void UnlockSoftwareAccess()
    {
        constexpr uint32_t lockedLock = lsrLockImplemented | lsrLocked;

        if ((Reg32(dwtLsr) & lockedLock) == lockedLock)
            Reg32(dwtLar) = larUnlockKey;
    }
}

namespace hal::cortex
{
    DataWatchpointAndTrace::DataWatchpointAndTrace()
    {
        Reg32(coreDebugDemcr) |= demcrTrcena;
        UnlockSoftwareAccess();
    }

    void DataWatchpointAndTrace::Start()
    {
        Reg32(dwtCyccnt) = 0;
        Reg32(dwtCtrl) |= dwtCyccntena;
    }

    void DataWatchpointAndTrace::Stop()
    {
        Reg32(dwtCtrl) &= ~dwtCyccntena;
    }

    uint32_t DataWatchpointAndTrace::Cycles() const
    {
        return Reg32(dwtCyccnt);
    }
}
