#ifndef SERVICES_HIL_DEADLINE_TIME_KEEPER_HPP
#define SERVICES_HIL_DEADLINE_TIME_KEEPER_HPP

#include "hal/synchronous_interfaces/TimeKeeper.hpp"
#include "infra/timer/Timer.hpp"

namespace services
{
    class HilDeadlineTimeKeeper
        : public hal::TimeKeeper
    {
    public:
        void Arm(infra::Duration duration);

        bool Timeout() override;
        void Reset() override;

    private:
        infra::TimePoint deadline;
    };
}

#endif
