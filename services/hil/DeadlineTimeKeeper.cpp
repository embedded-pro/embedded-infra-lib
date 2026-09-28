#include "services/hil/DeadlineTimeKeeper.hpp"

namespace services::hil
{
    void DeadlineTimeKeeper::Arm(infra::Duration duration)
    {
        deadline = infra::Now() + duration;
    }

    bool DeadlineTimeKeeper::Timeout()
    {
        return infra::Now() >= deadline;
    }

    void DeadlineTimeKeeper::Reset()
    {}
}
