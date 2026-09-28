#ifndef SERVICES_HIL_STATUS_HPP
#define SERVICES_HIL_STATUS_HPP

#include <cstdint>

namespace services
{
    enum class HilStatus : uint8_t
    {
        done,
        usage,
        pin,
        busy,
        notOpen,
        unsupported,
        range,
        timeout,
        failed,
    };

    const char* ToString(HilStatus status);
}

#endif
