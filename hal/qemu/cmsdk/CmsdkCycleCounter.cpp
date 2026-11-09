#include "hal/qemu/cmsdk/CmsdkCycleCounter.hpp"

namespace hal
{
    CmsdkCycleCounter::CmsdkCycleCounter(uintptr_t base)
        : timer(*reinterpret_cast<CmsdkTimerRegisters*>(base))
    {
        timer.control = 0;
        timer.reload = fullScale;
        timer.value = fullScale;
        timer.control = timerControlEnable;
    }

    CmsdkCycleCounter::~CmsdkCycleCounter()
    {
        timer.control = 0;
    }
}
