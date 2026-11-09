#ifndef HAL_QEMU_CMSDK_CMSDK_TIMER_HPP
#define HAL_QEMU_CMSDK_CMSDK_TIMER_HPP

#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/qemu/cmsdk/CmsdkTimerRegisters.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace hal
{
    class CmsdkTimer
        : private cortex::InterruptHandler
    {
    public:
        CmsdkTimer(uint32_t frequencyHz, const infra::Function<void()>& onTick,
            uintptr_t base = cmsdkTimer0BaseAddress,
            int32_t irqNumber = cmsdkTimer0IrqNumber,
            uint32_t clockHz = cmsdkTimerClockHz);
        CmsdkTimer(const CmsdkTimer&) = delete;
        CmsdkTimer& operator=(const CmsdkTimer&) = delete;
        ~CmsdkTimer();

        void Start();
        void Stop();

    private:
        void Invoke() override;

        CmsdkTimerRegisters& timer;
        infra::Function<void()> onTick;
    };
}

#endif
