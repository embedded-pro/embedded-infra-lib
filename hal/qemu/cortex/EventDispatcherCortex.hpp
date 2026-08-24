#ifndef HAL_QEMU_CORTEX_EVENT_DISPATCHER_CORTEX_HPP
#define HAL_QEMU_CORTEX_EVENT_DISPATCHER_CORTEX_HPP

#include "infra/event/EventDispatcherWithWeakPtr.hpp"

namespace hal::cortex
{
    class EventDispatcherCortexWorker
        : public infra::EventDispatcherWithWeakPtrWorker
    {
    public:
        using infra::EventDispatcherWithWeakPtrWorker::EventDispatcherWithWeakPtrWorker;

    protected:
        void RequestExecution() override;
        void Idle() override;
    };

    using EventDispatcherCortex =
        infra::EventDispatcherWithWeakPtrConnector<EventDispatcherCortexWorker>;
}

#endif
