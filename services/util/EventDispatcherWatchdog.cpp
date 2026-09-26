#include "services/util/EventDispatcherWatchdog.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace services::detail
{
    uint32_t EarlyWarningsUntilExpiry(infra::Duration expirationTimeout, infra::Duration earlyWarningPeriod)
    {
        really_assert(earlyWarningPeriod > infra::Duration::zero());
        auto count = (expirationTimeout + earlyWarningPeriod - infra::Duration(1)) / earlyWarningPeriod;
        return static_cast<uint32_t>(std::max<infra::Duration::rep>(count, 1));
    }
}
