#include "hal/qemu/cmsdk/CmsdkTimer.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace hal
{
    CmsdkTimer::CmsdkTimer(uint32_t frequencyHz, const infra::Function<void()>& onTick, uintptr_t base, int32_t irqNumber, uint32_t clockHz)
        : timer(*reinterpret_cast<CmsdkTimerRegisters*>(base))
        , onTick(onTick)
    {
        really_assert(frequencyHz != 0 && frequencyHz <= clockHz);

        timer.control = 0;
        timer.reload = clockHz / frequencyHz - 1;
        timer.value = timer.reload;
        Register(irqNumber);
    }

    CmsdkTimer::~CmsdkTimer()
    {
        Stop();
        timer.intClear = timerIntClear;
    }

    void CmsdkTimer::Start()
    {
        timer.control = timerControlEnable | timerControlIntEnable;
    }

    void CmsdkTimer::Stop()
    {
        timer.control = 0;
    }

    void CmsdkTimer::Invoke()
    {
        timer.intClear = timerIntClear;
        onTick();
    }
}
