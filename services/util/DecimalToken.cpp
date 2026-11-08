#include "services/util/DecimalToken.hpp"

namespace services
{
    namespace
    {
        constexpr std::size_t maxIntegerDigits{ 9 };
        constexpr std::size_t maxFractionDigits{ 6 };

        std::size_t SkipDigits(infra::BoundedConstString token, std::size_t index)
        {
            while (index != token.size() && token[index] >= '0' && token[index] <= '9')
                ++index;

            return index;
        }
    }

    bool IsDecimalToken(infra::BoundedConstString token, bool allowNegative, bool allowFraction)
    {
        std::size_t index{ 0 };
        if (allowNegative && index != token.size() && token[index] == '-')
            ++index;

        const auto integerEnd = SkipDigits(token, index);
        const auto integerDigits = integerEnd - index;
        if (integerDigits == 0 || integerDigits > maxIntegerDigits)
            return false;
        if (integerEnd == token.size())
            return true;
        if (!allowFraction || token[integerEnd] != '.')
            return false;

        const auto fractionEnd = SkipDigits(token, integerEnd + 1);
        const auto fractionDigits = fractionEnd - integerEnd - 1;
        return fractionEnd == token.size() && fractionDigits != 0 && fractionDigits <= maxFractionDigits;
    }

    template std::optional<uint8_t> ParseDecimal<uint8_t>(infra::BoundedConstString token, uint8_t min, uint8_t max);
    template std::optional<uint32_t> ParseDecimal<uint32_t>(infra::BoundedConstString token, uint32_t min, uint32_t max);
    template std::optional<int32_t> ParseDecimal<int32_t>(infra::BoundedConstString token, int32_t min, int32_t max);
    template std::optional<float> ParseDecimal<float>(infra::BoundedConstString token, float min, float max);
}
