#ifndef SERVICES_HIL_STATUS_HPP
#define SERVICES_HIL_STATUS_HPP

#include <cstdint>

namespace services::hil
{
    enum class Status : uint8_t
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

    const char* ToString(Status status);
}

#endif
