#ifndef HAL_CORTEX_M_DATA_WATCHPOINT_AND_TRACE_HPP
#define HAL_CORTEX_M_DATA_WATCHPOINT_AND_TRACE_HPP

#include <cstdint>

namespace hal::cortex
{
    class DataWatchpointAndTrace
    {
    public:
        DataWatchpointAndTrace();

        void Start();
        void Stop();

        uint32_t Cycles() const
        {
            return *reinterpret_cast<volatile uint32_t*>(cycleCount);
        }

    private:
        static constexpr uintptr_t cycleCount = 0xE0001004u;
    };
}

#endif
