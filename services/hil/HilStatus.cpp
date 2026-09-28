#include "services/hil/HilStatus.hpp"
#include <array>
#include <cstddef>

namespace services
{
    namespace
    {
        constexpr std::array<const char*, 9> reasons{ {
            "",
            "usage",
            "pin",
            "busy",
            "notopen",
            "unsupported",
            "range",
            "timeout",
            "failed",
        } };
    }

    const char* ToString(HilStatus status)
    {
        return reasons[static_cast<std::size_t>(status)];
    }
}
