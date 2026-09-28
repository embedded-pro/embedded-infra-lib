#include "services/hil/commands/HilPendingOperation.hpp"

namespace services
{
    HilPendingOperation::HilPendingOperation(HilResponse& response)
        : response(response)
    {}

    bool HilPendingOperation::Busy() const
    {
        return busy;
    }

    uint32_t HilPendingOperation::Start(infra::Duration timeout)
    {
        busy = true;
        timer.Start(timeout, [this]()
            {
                response.Error(HilStatus::timeout);
            });

        return ++generation;
    }

    bool HilPendingOperation::Complete(uint32_t operation)
    {
        if (operation != generation)
            return false;

        busy = false;

        if (!timer.Armed())
            return false;

        timer.Cancel();
        return true;
    }

    void HilPendingOperation::Cancel()
    {
        ++generation;
        busy = false;
        timer.Cancel();
    }
}
