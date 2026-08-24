#include "hal/qemu/cortex/EventDispatcherCortex.hpp"

namespace hal::cortex
{
    void EventDispatcherCortexWorker::RequestExecution()
    {}

    void EventDispatcherCortexWorker::Idle()
    {
        __asm volatile("wfi");
    }
}
