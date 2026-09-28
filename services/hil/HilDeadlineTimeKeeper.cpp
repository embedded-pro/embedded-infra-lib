#include "services/hil/HilDeadlineTimeKeeper.hpp"

namespace services
{
    void HilDeadlineTimeKeeper::Arm(infra::Duration duration)
    {
        deadline = infra::Now() + duration;
    }

    bool HilDeadlineTimeKeeper::Timeout()
    {
        return infra::Now() >= deadline;
    }

    void HilDeadlineTimeKeeper::Reset()
    {}
}
