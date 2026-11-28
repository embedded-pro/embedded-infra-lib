#include "hal/generic/TimerServiceGeneric.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace hal
{
    TimerServiceGeneric::TimerServiceGeneric(uint32_t id)
        : infra::TimerService(id)
        , lifetime(std::make_shared<Lifetime>(this))
        , nextTrigger(NextTrigger())
        , triggerThread([this]()
              {
                  WaitForTrigger();
              })
    {}

    TimerServiceGeneric::~TimerServiceGeneric()
    {
        {
            std::scoped_lock lock{ lifetime->mutex };
            lifetime->service = nullptr;
        }

        {
            // Under the lock, the trigger thread cannot test quit and miss the notify before it waits
            std::scoped_lock lock{ mutex };
            quit = true;
            condition.notify_one();
        }

        triggerThread.join();
    }

    void TimerServiceGeneric::NextTriggerChanged()
    {
        std::unique_lock<std::recursive_mutex> lock(mutex);
        nextTrigger = NextTrigger();
        condition.notify_one();
    }

    infra::TimePoint TimerServiceGeneric::Now() const
    {
        return std::chrono::system_clock::now();
    }

    infra::Duration TimerServiceGeneric::Resolution() const
    {
        return std::chrono::duration<long long, std::chrono::system_clock::period>(1);
    }

    void TimerServiceGeneric::Trigger()
    {
        std::unique_lock<std::recursive_mutex> eventLock(mutex);
        Progressed(Now());
        NextTriggerChanged();
    }

    void TimerServiceGeneric::WaitForTrigger()
    {
        std::unique_lock<std::recursive_mutex> lock(mutex);

        while (!quit)
        {
            if (nextTrigger != infra::TimePoint::max())
                condition.wait_until(lock, nextTrigger);
            else
                condition.wait(lock);

            if (!quit && nextTrigger != infra::TimePoint::max() && nextTrigger <= Now())
            {
                nextTrigger = infra::TimePoint::max();

                // A scheduled action can outlive the service: it reaches the service only through the lifetime token
                infra::EventDispatcher::Instance().Schedule([token = lifetime]()
                    {
                        std::scoped_lock tokenLock{ token->mutex };
                        if (token->service != nullptr)
                            token->service->Trigger();
                    });
            }
        }
    }
}
