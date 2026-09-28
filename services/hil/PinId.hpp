#ifndef SERVICES_HIL_PIN_ID_HPP
#define SERVICES_HIL_PIN_ID_HPP

#include <cstdint>

namespace services::hil
{
    struct PinId
    {
        uint8_t port = 0;
        uint8_t index = 0;

        constexpr bool operator==(const PinId& other) const = default;
    };

    enum class Pull : uint8_t
    {
        none,
        up,
        down,
    };

    struct PinAlias
    {
        const char* name;
        PinId pin;
        Pull pull = Pull::none;
    };
}

#endif
