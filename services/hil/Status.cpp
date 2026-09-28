#include "services/hil/Status.hpp"
#include <array>
#include <cstddef>

namespace services::hil
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

    const char* ToString(Status status)
    {
        return reasons[static_cast<std::size_t>(status)];
    }
}
