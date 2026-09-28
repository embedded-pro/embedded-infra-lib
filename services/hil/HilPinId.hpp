#ifndef SERVICES_HIL_PIN_ID_HPP
#define SERVICES_HIL_PIN_ID_HPP

#include <cstdint>

namespace services
{
    struct HilPinId
    {
        uint8_t port = 0;
        uint8_t index = 0;

        constexpr bool operator==(const HilPinId& other) const = default;
    };

    enum class HilPull : uint8_t
    {
        none,
        up,
        down,
    };

    struct HilPinAlias
    {
        const char* name;
        HilPinId pin;
        HilPull pull = HilPull::none;
    };
}

#endif
