#ifndef HAL_CORTEX_M_EVENT_DISPATCHER_CORTEX_HPP
#define HAL_CORTEX_M_EVENT_DISPATCHER_CORTEX_HPP

#include "infra/event/EventDispatcher.hpp"

namespace hal::cortex
{
    class EventDispatcherCortexWorker
        : public infra::EventDispatcherWorkerImpl
    {
    public:
        explicit EventDispatcherCortexWorker(infra::MemoryRange<std::pair<infra::Function<void()>, std::atomic<bool>>> scheduledActionsStorage);

    protected:
        void Idle() override;
        void RequestExecution() override;
    };

    using EventDispatcherCortex = infra::EventDispatcherConnector<EventDispatcherCortexWorker>;
}

#endif
