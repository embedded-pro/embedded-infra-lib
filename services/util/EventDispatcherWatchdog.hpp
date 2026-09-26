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
    namespace detail
    {
        uint32_t EarlyWarningsUntilExpiry(infra::Duration expirationTimeout, infra::Duration earlyWarningPeriod);
    }

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
        void Step();
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

    using EventDispatcherWithWatchdog = infra::EventDispatcherConnector<EventDispatcherWatchdogWorker<infra::EventDispatcherWorkerImpl>>;
    using EventDispatcherWithWeakPtrAndWatchdog = infra::EventDispatcherWithWeakPtrConnector<EventDispatcherWatchdogWorker<infra::EventDispatcherWithWeakPtrWorker>>;
    using LowPowerEventDispatcherWithWeakPtrAndWatchdog = infra::EventDispatcherWithWeakPtrConnector<EventDispatcherWatchdogWorker<infra::LowPowerEventDispatcherWorker>>;

    ////    Implementation    ////

    template<class Worker>
    template<class ScheduledActionsStorage, class... WorkerArgs>
    EventDispatcherWatchdogWorker<Worker>::EventDispatcherWatchdogWorker(ScheduledActionsStorage scheduledActionsStorage, hal::Watchdog& watchdog, infra::Duration expirationTimeout, const infra::Function<void()>& onExpired, WorkerArgs&&... workerArgs)
        : Worker(scheduledActionsStorage, std::forward<WorkerArgs>(workerArgs)...)
        , watchdog(watchdog)
        , expirationCount(detail::EarlyWarningsUntilExpiry(expirationTimeout, watchdog.EarlyWarningPeriod()))
        , onExpired(onExpired)
    {
        watchdog.Start([this]()
            {
                EarlyWarning();
            });
    }

    template<class Worker>
    void EventDispatcherWatchdogWorker<Worker>::ExecuteFirstAction()
    {
        Step();
        Worker::ExecuteFirstAction();
        Step();
    }

    template<class Worker>
    void EventDispatcherWatchdogWorker<Worker>::Step()
    {
        steps.store(steps.load() + 1);
    }

    template<class Worker>
    bool EventDispatcherWatchdogWorker<Worker>::Progressed()
    {
        auto current = steps.load();
        auto progressed = current % 2 == 0 || current != stepsAtLastEarlyWarning;
        stepsAtLastEarlyWarning = current;
        return progressed;
    }

    template<class Worker>
    void EventDispatcherWatchdogWorker<Worker>::EarlyWarning()
    {
        if (expired)
            return;

        watchdog.Refresh();

        if (Progressed())
            missedEarlyWarnings = 0;
        else if (++missedEarlyWarnings == expirationCount)
        {
            expired = true;
            onExpired();
        }
    }
}

#endif
