#ifndef HAL_TIMER_SERVICE_GENERIC_HPP
#define HAL_TIMER_SERVICE_GENERIC_HPP

#include "infra/timer/TimerService.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace hal
{
    class TimerServiceGeneric
        : public infra::TimerService
    {
    public:
        explicit TimerServiceGeneric(uint32_t id = infra::systemTimerServiceId);
        TimerServiceGeneric(const TimerServiceGeneric& other) = delete;
        ~TimerServiceGeneric();
        TimerServiceGeneric& operator=(const TimerServiceGeneric& other) = delete;

        void NextTriggerChanged() override;
        infra::TimePoint Now() const override;
        infra::Duration Resolution() const override;

    private:
        struct Lifetime
        {
            explicit Lifetime(TimerServiceGeneric* service)
                : service(service)
            {}

            std::recursive_mutex mutex;
            TimerServiceGeneric* service;
        };

        void WaitForTrigger();
        void Trigger();

    private:
        std::shared_ptr<Lifetime> lifetime;
        std::condition_variable_any condition;
        std::recursive_mutex mutex;
        infra::TimePoint nextTrigger;
        bool quit = false;
        std::thread triggerThread;
    };
}

#endif
