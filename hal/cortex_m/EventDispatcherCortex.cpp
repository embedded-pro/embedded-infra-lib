#include "hal/cortex_m/EventDispatcherCortex.hpp"

namespace
{
    void Dsb() { __asm volatile("dsb" ::: "memory"); }
    void Wfe() { __asm volatile("wfe"); }
    void Sev() { __asm volatile("sev"); }
}

namespace hal::cortex
{
    EventDispatcherCortexWorker::EventDispatcherCortexWorker(infra::MemoryRange<std::pair<infra::Function<void()>, std::atomic<bool>>> scheduledActionsStorage)
        : infra::EventDispatcherWorkerImpl(scheduledActionsStorage)
    {}

    void EventDispatcherCortexWorker::Idle()
    {
        Dsb();
        Wfe();
    }

    void EventDispatcherCortexWorker::RequestExecution()
    {
        Dsb();
        Sev();
    }
}
