#include "services/hil/HilPinNaming.hpp"

namespace services
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

    HilPinNamingDefault::HilPinNamingDefault(infra::BoundedConstString portLetters, uint8_t maximumIndex, infra::MemoryRange<const HilPinAlias> aliases)
        : portLetters(portLetters)
        , maximumIndex(maximumIndex)
        , aliases(aliases)
    {}

    std::optional<HilPinId> HilPinNamingDefault::Parse(infra::BoundedConstString text, HilPull& aliasPull) const
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
        if (!port.has_value() || !index.has_value())
            return std::nullopt;

        return HilPinId{ *port, *index };
    }

    void HilPinNamingDefault::Print(infra::TextOutputStream& stream, HilPinId pin) const
    {
        stream << 'P' << portLetters[pin.port] << static_cast<uint32_t>(pin.index);
    }

    infra::MemoryRange<const HilPinAlias> HilPinNamingDefault::Aliases() const
    {
        return aliases;
    }

    std::optional<uint8_t> HilPinNamingDefault::ParsePort(char letter) const
    {
        letter = ToUpper(letter);
        if (letter < 'A' || letter > 'Z')
            return std::nullopt;

        auto position = portLetters.find(letter);
        if (position == infra::BoundedConstString::npos)
            return std::nullopt;

        return static_cast<uint8_t>(position);
    }

    std::optional<uint8_t> HilPinNamingDefault::ParseIndex(infra::BoundedConstString text) const
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
