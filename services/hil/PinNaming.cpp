#include "services/hil/PinNaming.hpp"

namespace services::hil
{
    namespace
    {
        constexpr std::size_t maximumIndexDigits = 3;

        char ToUpper(char letter)
        {
            if (letter >= 'a' && letter <= 'z')
                return static_cast<char>(letter - 'a' + 'A');

            return letter;
        }
    }

    PinNamingDefault::PinNamingDefault(infra::BoundedConstString portLetters, uint8_t maximumIndex, infra::MemoryRange<const PinAlias> aliases)
        : portLetters(portLetters)
        , maximumIndex(maximumIndex)
        , aliases(aliases)
    {}

    std::optional<PinId> PinNamingDefault::Parse(infra::BoundedConstString text, Pull& aliasPull) const
    {
        for (const auto& alias : aliases)
            if (text == alias.name)
            {
                aliasPull = alias.pull;
                return alias.pin;
            }

        if (text.size() < 3 || ToUpper(text[0]) != 'P')
            return std::nullopt;

        auto port = ParsePort(text[1]);
        auto index = ParseIndex(text.substr(2));
        if (!port || !index)
            return std::nullopt;

        return PinId{ *port, *index };
    }

    void PinNamingDefault::Print(infra::TextOutputStream& stream, PinId pin) const
    {
        stream << 'P' << portLetters[pin.port] << static_cast<uint32_t>(pin.index);
    }

    infra::MemoryRange<const PinAlias> PinNamingDefault::Aliases() const
    {
        return aliases;
    }

    std::optional<uint8_t> PinNamingDefault::ParsePort(char letter) const
    {
        letter = ToUpper(letter);
        if (letter < 'A' || letter > 'Z')
            return std::nullopt;

        auto position = portLetters.find(letter);
        if (position == infra::BoundedConstString::npos)
            return std::nullopt;

        return static_cast<uint8_t>(position);
    }

    std::optional<uint8_t> PinNamingDefault::ParseIndex(infra::BoundedConstString text) const
    {
        if (text.empty() || text.size() > maximumIndexDigits || (text.size() > 1 && text[0] == '0'))
            return std::nullopt;

        uint32_t index = 0;
        for (auto digit : text)
        {
            if (digit < '0' || digit > '9')
                return std::nullopt;

            index = index * 10 + static_cast<uint32_t>(digit - '0');
        }

        if (index > maximumIndex)
            return std::nullopt;

        return static_cast<uint8_t>(index);
    }
}
