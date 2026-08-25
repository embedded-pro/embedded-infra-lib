#include "hal/cortex_m/DataWatchpointAndTrace.hpp"

#if !defined(__ARM_ARCH_6M__)

namespace
{
    constexpr uintptr_t demcrAddr = 0xE000EDFCu;
    constexpr uint32_t demcrTrcena = 1u << 24;

    constexpr uintptr_t dwtCtrlAddr = 0xE0001000u;
    constexpr uintptr_t dwtCyccntAddr = 0xE0001004u;
    constexpr uint32_t dwtCtrlCyccntena = 1u << 0;

    volatile uint32_t& Reg32(uintptr_t address)
    {
        return *reinterpret_cast<volatile uint32_t*>(address);
    }
}

namespace hal::cortex
{
    DataWatchpointAndTrace::DataWatchpointAndTrace()
    {
        Reg32(demcrAddr) |= demcrTrcena;
        Reg32(dwtCyccntAddr) = 0;
    }

    DataWatchpointAndTrace::~DataWatchpointAndTrace()
    {
        Reg32(demcrAddr) &= ~demcrTrcena;
    }

    void DataWatchpointAndTrace::Start() const
    {
        Reg32(dwtCyccntAddr) = 0;
        Reg32(dwtCtrlAddr) |= dwtCtrlCyccntena;
    }

    uint32_t DataWatchpointAndTrace::Stop() const
    {
        uint32_t cycles = Reg32(dwtCyccntAddr);
        Reg32(dwtCtrlAddr) &= ~dwtCtrlCyccntena;
        return cycles;
    }
}

#endif
