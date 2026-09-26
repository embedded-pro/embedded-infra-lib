#ifndef SERVICES_EVENT_DISPATCHER_WATCHDOG_HPP
#define SERVICES_EVENT_DISPATCHER_WATCHDOG_HPP

#include "hal/interfaces/Watchdog.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/event/EventDispatcherWithWeakPtr.hpp"
#include "infra/event/LowPowerEventDispatcher.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace services
{
    class EventDispatcherWatchdogSupervision
    {
    public:
        EventDispatcherWatchdogSupervision(hal::Watchdog& watchdog, infra::Duration expirationTimeout, const infra::Function<void()>& onExpired);
        EventDispatcherWatchdogSupervision(const EventDispatcherWatchdogSupervision& other) = delete;
        EventDispatcherWatchdogSupervision& operator=(const EventDispatcherWatchdogSupervision& other) = delete;
        ~EventDispatcherWatchdogSupervision() = default;

        void Step();

    private:
        bool Progressed();
        void EarlyWarning();

        hal::Watchdog& watchdog;
        uint32_t expirationCount;
        infra::Function<void()> onExpired;
        std::atomic<uint32_t> steps{ 0 };
        uint32_t stepsAtLastEarlyWarning{ 0 };
        uint32_t missedEarlyWarnings{ 0 };
        bool expired{ false };
    };

    template<class Worker>
    class EventDispatcherWatchdogWorker
        : public Worker
    {
    public:
        template<std::size_t StorageSize, class T = EventDispatcherWatchdogWorker>
        using WithSize = typename Worker::template WithSize<StorageSize, T>;

        template<class ScheduledActionsStorage, class... WorkerArgs>
        EventDispatcherWatchdogWorker(ScheduledActionsStorage scheduledActionsStorage, hal::Watchdog& watchdog, infra::Duration expirationTimeout, const infra::Function<void()>& onExpired, WorkerArgs&&... workerArgs);

        void ExecuteFirstAction() override;

    private:
        EventDispatcherWatchdogSupervision supervision;
    };

    extern template class EventDispatcherWatchdogWorker<infra::EventDispatcherWorkerImpl>;
    extern template class EventDispatcherWatchdogWorker<infra::EventDispatcherWithWeakPtrWorker>;
    extern template class EventDispatcherWatchdogWorker<infra::LowPowerEventDispatcherWorker>;

    using EventDispatcherWithWatchdog = infra::EventDispatcherConnector<EventDispatcherWatchdogWorker<infra::EventDispatcherWorkerImpl>>;
    using EventDispatcherWithWeakPtrAndWatchdog = infra::EventDispatcherWithWeakPtrConnector<EventDispatcherWatchdogWorker<infra::EventDispatcherWithWeakPtrWorker>>;
    using LowPowerEventDispatcherWithWeakPtrAndWatchdog = infra::EventDispatcherWithWeakPtrConnector<EventDispatcherWatchdogWorker<infra::LowPowerEventDispatcherWorker>>;

    ////    Implementation    ////

    template<class Worker>
    template<class ScheduledActionsStorage, class... WorkerArgs>
    EventDispatcherWatchdogWorker<Worker>::EventDispatcherWatchdogWorker(ScheduledActionsStorage scheduledActionsStorage, hal::Watchdog& watchdog, infra::Duration expirationTimeout, const infra::Function<void()>& onExpired, WorkerArgs&&... workerArgs)
        : Worker(scheduledActionsStorage, std::forward<WorkerArgs>(workerArgs)...)
        , supervision(watchdog, expirationTimeout, onExpired)
    {}

    template<class Worker>
    void EventDispatcherWatchdogWorker<Worker>::ExecuteFirstAction()
    {
        supervision.Step();
        Worker::ExecuteFirstAction();
        supervision.Step();
    }
}

#endif
