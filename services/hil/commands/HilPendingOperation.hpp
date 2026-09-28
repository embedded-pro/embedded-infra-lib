#ifndef SERVICES_HIL_PENDING_OPERATION_HPP
#define SERVICES_HIL_PENDING_OPERATION_HPP

#include "infra/timer/Timer.hpp"
#include "services/hil/HilResponse.hpp"
#include <cstdint>

namespace services
{
    class HilPendingOperation
    {
    public:
        explicit HilPendingOperation(HilResponse& response);
        HilPendingOperation(const HilPendingOperation& other) = delete;
        HilPendingOperation& operator=(const HilPendingOperation& other) = delete;
        ~HilPendingOperation() = default;

        bool Busy() const;
        uint32_t Start(infra::Duration timeout);
        bool Complete(uint32_t operation);
        void Cancel();

    private:
        HilResponse& response;
        uint32_t generation = 0;
        bool busy = false;
        infra::TimerSingleShot timer;
    };
}

#endif
