#ifndef SERVICES_HIL_PIN_NAMING_HPP
#define SERVICES_HIL_PIN_NAMING_HPP

#include "infra/stream/OutputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/hil/PinId.hpp"
#include <cstdint>
#include <optional>

namespace services::hil
{
    class PinNaming
    {
    protected:
        PinNaming() = default;
        PinNaming(const PinNaming& other) = delete;
        PinNaming& operator=(const PinNaming& other) = delete;
        ~PinNaming() = default;

    public:
        virtual std::optional<PinId> Parse(infra::BoundedConstString text, Pull& aliasPull) const = 0;
        virtual void Print(infra::TextOutputStream& stream, PinId pin) const = 0;
        virtual infra::MemoryRange<const PinAlias> Aliases() const = 0;
    };

    class PinNamingDefault
        : public PinNaming
    {
    public:
        PinNamingDefault(infra::BoundedConstString portLetters, uint8_t maximumIndex, infra::MemoryRange<const PinAlias> aliases = {});

        std::optional<PinId> Parse(infra::BoundedConstString text, Pull& aliasPull) const override;
        void Print(infra::TextOutputStream& stream, PinId pin) const override;
        infra::MemoryRange<const PinAlias> Aliases() const override;

    private:
        std::optional<uint8_t> ParsePort(char letter) const;
        std::optional<uint8_t> ParseIndex(infra::BoundedConstString text) const;

    private:
        infra::BoundedConstString portLetters;
        uint8_t maximumIndex;
        infra::MemoryRange<const PinAlias> aliases;
    };
}

#endif
