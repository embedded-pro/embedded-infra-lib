#ifndef HAL_QEMU_CMSDK_CMSDK_CYCLE_COUNTER_HPP
#define HAL_QEMU_CMSDK_CMSDK_CYCLE_COUNTER_HPP

#include "hal/qemu/cmsdk/CmsdkTimerRegisters.hpp"
#include <cstdint>

namespace hal
{
    class CmsdkCycleCounter
    {
    public:
        explicit CmsdkCycleCounter(uintptr_t base = cmsdkTimer1BaseAddress);
        CmsdkCycleCounter(const CmsdkCycleCounter&) = delete;
        CmsdkCycleCounter& operator=(const CmsdkCycleCounter&) = delete;
        ~CmsdkCycleCounter();

        uint32_t Now() const
        {
            return fullScale - timer.value;
        }

    private:
        static constexpr uint32_t fullScale = 0xffffffffu;

        CmsdkTimerRegisters& timer;
    };
}

#endif
