#include "services/hil/Arguments.hpp"
#include <cstring>
#include <limits>

namespace services::hil
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

    std::optional<uint32_t> ParseNumber(infra::BoundedConstString text)
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

    std::optional<hal::DutyCycle> ParseDutyCycle(infra::BoundedConstString text)
    {
        auto dot = text.find('.');
        auto integerText = text.substr(0, dot);
        auto integerPart = ParseNumber(integerText);
        if (!integerPart || integerText.find('x') != infra::BoundedConstString::npos || *integerPart > 100)
            return std::nullopt;

        uint64_t fraction = 0;
        if (dot != infra::BoundedConstString::npos)
        {
            auto parsed = ParseFraction(text.substr(dot + 1));
            if (!parsed)
                return std::nullopt;

            fraction = *parsed;
        }

        auto scaled = *integerPart * fractionScale + fraction;
        if (scaled > 100 * fractionScale)
            return std::nullopt;

        return hal::DutyCycle(static_cast<uint32_t>((scaled * hal::DutyCycle::fullScale + 50 * fractionScale) / (100 * fractionScale)));
    }

    std::optional<PinId> ParsePin(infra::BoundedConstString text, const PinNaming& naming)
    {
        Pull aliasPull = Pull::none;
        return naming.Parse(text, aliasPull);
    }

    Status ParseHex(infra::BoundedConstString text, infra::ByteRange output, std::size_t& size)
    {
        size = 0;

        if (text == "-")
            return Status::done;

        if (text.empty() || text.size() % 2 != 0)
            return Status::usage;

        if (text.size() / 2 > output.size())
            return Status::range;

        for (std::size_t i = 0; i != text.size(); i += 2)
        {
            auto high = HexDigit(text[i]);
            auto low = HexDigit(text[i + 1]);
            if (!high || !low)
                return Status::usage;

            output[size++] = static_cast<uint8_t>((*high << 4) | *low);
        }

        return Status::done;
    }

    Arguments::Arguments(infra::BoundedConstString parameters)
        : tokenizer(parameters, ' ')
        , tokens(tokenizer.Size())
    {}

    bool Arguments::Shape(std::size_t minimumPositional, std::size_t maximumPositional, std::initializer_list<const char*> keys) const
    {
        auto positional = PositionalCount();
        return positional >= minimumPositional && positional <= maximumPositional && KnownKeys(keys);
    }

    bool Arguments::Shape(std::size_t minimumPositional, std::size_t maximumPositional, infra::MemoryRange<const char* const> keys) const
    {
        auto positional = PositionalCount();
        return positional >= minimumPositional && positional <= maximumPositional && KnownKeys(keys);
    }

    std::size_t Arguments::PositionalCount() const
    {
        std::size_t count = 0;

        for (std::size_t i = 0; i != tokens; ++i)
            if (!IsKeyValue(tokenizer.Token(i)))
                ++count;

        return count;
    }

    infra::BoundedConstString Arguments::Positional(std::size_t index) const
    {
        for (std::size_t i = 0; i != tokens; ++i)
        {
            auto token = tokenizer.Token(i);
            if (!IsKeyValue(token) && index-- == 0)
                return token;
        }

        return infra::BoundedConstString();
    }

    std::optional<infra::BoundedConstString> Arguments::Key(const char* key) const
    {
        auto length = std::strlen(key);

        for (std::size_t i = 0; i != tokens; ++i)
        {
            auto token = tokenizer.Token(i);
            if (token.size() > length && token[length] == '=' && token.substr(0, length) == key)
                return token.substr(length + 1);
        }

        return std::nullopt;
    }

    bool Arguments::Has(const char* key) const
    {
        return Key(key).has_value();
    }

    void Arguments::NumberAt(std::size_t index, uint32_t& value, uint32_t minimum, uint32_t maximum, Status& status) const
    {
        if (status == Status::done)
            Number(Positional(index), value, minimum, maximum, status);
    }

    void Arguments::Number(const char* key, uint32_t& value, uint32_t minimum, uint32_t maximum, Status& status) const
    {
        if (status != Status::done)
            return;

        if (auto text = Key(key))
            Number(*text, value, minimum, maximum, status);
    }

    void Arguments::Flag(const char* key, bool& value, Status& status) const
    {
        uint32_t number = value ? 1 : 0;
        Number(key, number, 0, 1, status);
        value = number != 0;
    }

    void Arguments::Pin(const char* key, const PinNaming& naming, std::optional<PinId>& pin, Status& status) const
    {
        if (status != Status::done)
            return;

        if (auto text = Key(key))
        {
            pin = ParsePin(*text, naming);
            if (!pin)
                status = Status::pin;
        }
    }

    void Arguments::PinAt(std::size_t index, const PinNaming& naming, PinId& pin, Status& status) const
    {
        Pull aliasPull = Pull::none;
        PinAt(index, naming, pin, aliasPull, status);
    }

    void Arguments::PinAt(std::size_t index, const PinNaming& naming, PinId& pin, Pull& aliasPull, Status& status) const
    {
        if (status != Status::done)
            return;

        if (auto parsed = naming.Parse(Positional(index), aliasPull))
            pin = *parsed;
        else
            status = Status::pin;
    }

    void Arguments::Number(infra::BoundedConstString text, uint32_t& value, uint32_t minimum, uint32_t maximum, Status& status)
    {
        auto number = ParseNumber(text);

        if (!number)
            status = Status::usage;
        else if (*number < minimum || *number > maximum)
            status = Status::range;
        else
            value = *number;
    }

    bool Arguments::IsKeyValue(infra::BoundedConstString token)
    {
        return token.find('=') != infra::BoundedConstString::npos;
    }
}
