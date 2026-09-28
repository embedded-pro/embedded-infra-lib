#ifndef SERVICES_HIL_PIN_NAMING_HPP
#define SERVICES_HIL_PIN_NAMING_HPP

#include "infra/stream/OutputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/hil/HilPinId.hpp"
#include <cstdint>
#include <optional>

namespace services
{
    class HilPinNaming
    {
    protected:
        HilPinNaming() = default;
        HilPinNaming(const HilPinNaming& other) = delete;
        HilPinNaming& operator=(const HilPinNaming& other) = delete;
        ~HilPinNaming() = default;

    public:
        virtual std::optional<HilPinId> Parse(infra::BoundedConstString text, HilPull& aliasPull) const = 0;
        virtual void Print(infra::TextOutputStream& stream, HilPinId pin) const = 0;
        virtual infra::MemoryRange<const HilPinAlias> Aliases() const = 0;
    };

    class HilPinNamingDefault
        : public HilPinNaming
    {
    public:
        HilPinNamingDefault(infra::BoundedConstString portLetters, uint8_t maximumIndex, infra::MemoryRange<const HilPinAlias> aliases = {});

        std::optional<HilPinId> Parse(infra::BoundedConstString text, HilPull& aliasPull) const override;
        void Print(infra::TextOutputStream& stream, HilPinId pin) const override;
        infra::MemoryRange<const HilPinAlias> Aliases() const override;

    private:
        std::optional<uint8_t> ParsePort(char letter) const;
        std::optional<uint8_t> ParseIndex(infra::BoundedConstString text) const;

    private:
        infra::BoundedConstString portLetters;
        uint8_t maximumIndex;
        infra::MemoryRange<const HilPinAlias> aliases;
    };
}

#endif
