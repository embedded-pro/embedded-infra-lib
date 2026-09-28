#include "services/hil/HilArguments.hpp"
#include <limits>

namespace services
{
    namespace
    {
        constexpr std::size_t fractionDigits = 4;
        constexpr uint64_t fractionScale = 10000;

        std::optional<uint8_t> HexDigit(char c)
        {
            if (c >= '0' && c <= '9')
                return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f')
                return static_cast<uint8_t>(c - 'a' + 10);
            if (c >= 'A' && c <= 'F')
                return static_cast<uint8_t>(c - 'A' + 10);

            return std::nullopt;
        }

        std::optional<uint64_t> ParseFraction(infra::BoundedConstString text)
        {
            if (text.empty() || text.size() > fractionDigits)
                return std::nullopt;

            uint64_t fraction = 0;
            for (std::size_t i = 0; i != fractionDigits; ++i)
            {
                fraction *= 10;

                if (i < text.size())
                {
                    if (text[i] < '0' || text[i] > '9')
                        return std::nullopt;

                    fraction += static_cast<uint64_t>(text[i] - '0');
                }
            }

            return fraction;
        }
    }

    std::optional<uint32_t> HilArguments::ParseNumber(infra::BoundedConstString text)
    {
        uint32_t base = 10;

        if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        {
            base = 16;
            text = text.substr(2);
        }

        if (text.empty())
            return std::nullopt;

        uint64_t value = 0;
        for (auto c : text)
        {
            auto digit = HexDigit(c);
            if (!digit || *digit >= base)
                return std::nullopt;

            value = value * base + *digit;
            if (value > std::numeric_limits<uint32_t>::max())
                return std::nullopt;
        }

        return static_cast<uint32_t>(value);
    }

    std::optional<hal::DutyCycle> HilArguments::ParseDutyCycle(infra::BoundedConstString text)
    {
        auto dot = text.find('.');
        auto integerText = text.substr(0, dot);
        auto integerPart = ParseNumber(integerText);
        if (!integerPart.has_value() || integerText.find('x') != infra::BoundedConstString::npos || *integerPart > 100)
            return std::nullopt;

        uint64_t fraction = 0;
        if (dot != infra::BoundedConstString::npos)
        {
            auto parsed = ParseFraction(text.substr(dot + 1));
            if (!parsed.has_value())
                return std::nullopt;

            fraction = *parsed;
        }

        auto scaled = *integerPart * fractionScale + fraction;
        if (scaled > 100 * fractionScale)
            return std::nullopt;

        return hal::DutyCycle::FromRatio(scaled, 100 * fractionScale);
    }

    std::optional<HilPinId> HilArguments::ParsePin(infra::BoundedConstString text, const HilPinNaming& naming)
    {
        HilPull aliasPull = HilPull::none;
        return naming.Parse(text, aliasPull);
    }

    HilStatus HilArguments::ParseHex(infra::BoundedConstString text, infra::ByteRange output, std::size_t& size)
    {
        size = 0;

        if (text == "-")
            return HilStatus::done;

        if (text.empty() || text.size() % 2 != 0)
            return HilStatus::usage;

        if (text.size() / 2 > output.size())
            return HilStatus::range;

        for (std::size_t i = 0; i != text.size(); i += 2)
        {
            auto high = HexDigit(text[i]);
            auto low = HexDigit(text[i + 1]);
            if (!high.has_value() || !low.has_value())
                return HilStatus::usage;

            output[size++] = static_cast<uint8_t>((*high << 4) | *low);
        }

        return HilStatus::done;
    }

    HilArguments::HilArguments(infra::BoundedConstString parameters)
        : tokenizer(parameters, ' ')
        , tokens(tokenizer.Size())
    {}

    bool HilArguments::Shape(std::size_t minimumPositional, std::size_t maximumPositional, std::initializer_list<const char*> keys) const
    {
        return Shape(minimumPositional, maximumPositional, infra::MakeRange(keys.begin(), keys.end()));
    }

    bool HilArguments::Shape(std::size_t minimumPositional, std::size_t maximumPositional, infra::MemoryRange<const char* const> keys) const
    {
        auto positional = PositionalCount();
        return positional >= minimumPositional && positional <= maximumPositional && KnownKeys(keys);
    }

    std::size_t HilArguments::PositionalCount() const
    {
        std::size_t count = 0;

        for (std::size_t i = 0; i != tokens; ++i)
            if (!IsKeyValue(tokenizer.Token(i)))
                ++count;

        return count;
    }

    infra::BoundedConstString HilArguments::Positional(std::size_t index) const
    {
        for (std::size_t i = 0; i != tokens; ++i)
        {
            auto token = tokenizer.Token(i);
            if (IsKeyValue(token))
                continue;

            if (index == 0)
                return token;

            --index;
        }

        return infra::BoundedConstString();
    }

    std::optional<infra::BoundedConstString> HilArguments::Key(const char* key) const
    {
        infra::BoundedConstString name(key);
        auto length = name.size();

        for (std::size_t i = 0; i != tokens; ++i)
        {
            auto token = tokenizer.Token(i);
            if (token.size() > length && token[length] == '=' && token.substr(0, length) == name)
                return token.substr(length + 1);
        }

        return std::nullopt;
    }

    bool HilArguments::Has(const char* key) const
    {
        return Key(key).has_value();
    }

    void HilArguments::NumberAt(std::size_t index, uint32_t& value, uint32_t minimum, uint32_t maximum, HilStatus& status) const
    {
        if (status == HilStatus::done)
            Number(Positional(index), value, minimum, maximum, status);
    }

    void HilArguments::Number(const char* key, uint32_t& value, uint32_t minimum, uint32_t maximum, HilStatus& status) const
    {
        if (status != HilStatus::done)
            return;

        if (auto text = Key(key))
            Number(*text, value, minimum, maximum, status);
    }

    void HilArguments::Flag(const char* key, bool& value, HilStatus& status) const
    {
        uint32_t number = value ? 1 : 0;
        Number(key, number, 0, 1, status);
        value = number != 0;
    }

    void HilArguments::Pin(const char* key, const HilPinNaming& naming, std::optional<HilPinId>& pin, HilStatus& status) const
    {
        if (status != HilStatus::done)
            return;

        if (auto text = Key(key))
        {
            pin = ParsePin(*text, naming);
            if (!pin)
                status = HilStatus::pin;
        }
    }

    void HilArguments::PinAt(std::size_t index, const HilPinNaming& naming, HilPinId& pin, HilStatus& status) const
    {
        HilPull aliasPull = HilPull::none;
        PinAt(index, naming, pin, aliasPull, status);
    }

    void HilArguments::PinAt(std::size_t index, const HilPinNaming& naming, HilPinId& pin, HilPull& aliasPull, HilStatus& status) const
    {
        if (status != HilStatus::done)
            return;

        if (auto parsed = naming.Parse(Positional(index), aliasPull))
            pin = *parsed;
        else
            status = HilStatus::pin;
    }

    void HilArguments::Number(infra::BoundedConstString text, uint32_t& value, uint32_t minimum, uint32_t maximum, HilStatus& status)
    {
        auto number = ParseNumber(text);

        if (!number.has_value())
            status = HilStatus::usage;
        else if (*number < minimum || *number > maximum)
            status = HilStatus::range;
        else
            value = *number;
    }

    bool HilArguments::KnownKeys(infra::MemoryRange<const char* const> keys) const
    {
        for (std::size_t i = 0; i != tokens; ++i)
        {
            auto token = tokenizer.Token(i);
            if (!IsKeyValue(token))
                continue;

            auto name = token.substr(0, token.find('='));
            bool known = false;
            for (auto key : keys)
                known = known || name == key;

            if (!known)
                return false;
        }

        return true;
    }

    bool HilArguments::IsKeyValue(infra::BoundedConstString token)
    {
        return token.find('=') != infra::BoundedConstString::npos;
    }
}
