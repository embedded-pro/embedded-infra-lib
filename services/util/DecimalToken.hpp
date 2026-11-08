#ifndef SERVICES_DECIMAL_TOKEN_HPP
#define SERVICES_DECIMAL_TOKEN_HPP

#include "infra/stream/StringInputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace services
{
    bool IsDecimalToken(infra::BoundedConstString token, bool allowNegative = true, bool allowFraction = true);

    template<class T>
    std::optional<T> ParseDecimal(infra::BoundedConstString token, T min, T max)
    {
        constexpr bool isFloat{ std::is_floating_point_v<T> };
        if (!IsDecimalToken(token, isFloat || std::is_signed_v<T>, isFloat))
            return std::nullopt;

        infra::StringInputStream stream{ token, infra::softFail };
        using Parsed = std::conditional_t<isFloat, float, int64_t>;
        Parsed value{};

        if (isFloat && token.find('.') == infra::BoundedConstString::npos)
        {
            int64_t whole{};
            stream >> whole;
            value = static_cast<Parsed>(whole);
        }
        else
            stream >> value;

        if (stream.ErrorPolicy().Failed() || value < static_cast<Parsed>(min) || value > static_cast<Parsed>(max))
            return std::nullopt;

        return static_cast<T>(value);
    }
}

#endif
