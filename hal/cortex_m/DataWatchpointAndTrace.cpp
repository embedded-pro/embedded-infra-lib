#include "hal/cortex_m/DataWatchpointAndTrace.hpp"

namespace
{
    constexpr uintptr_t coreDebugDemcr = 0xE000EDFCu;
    constexpr uint32_t demcrTrcena = 1u << 24;

    constexpr uintptr_t dwtCtrl = 0xE0001000u;
    constexpr uint32_t dwtCyccntena = 1u << 0;

    constexpr uintptr_t dwtCyccnt = 0xE0001004u;

    volatile uint32_t& Reg32(uintptr_t address)
    {
        return *reinterpret_cast<volatile uint32_t*>(address);
    }
}

namespace hal::cortex
{
    DataWatchpointAndTrace::DataWatchpointAndTrace()
    {
        Reg32(coreDebugDemcr) |= demcrTrcena;
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
