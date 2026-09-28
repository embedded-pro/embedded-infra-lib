#ifndef SERVICES_HIL_ARGUMENTS_HPP
#define SERVICES_HIL_ARGUMENTS_HPP

#include "hal/interfaces/DutyCycle.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/Tokenizer.hpp"
#include "services/hil/HilPinId.hpp"
#include "services/hil/HilPinNaming.hpp"
#include "services/hil/HilStatus.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>

namespace services
{
    template<class T>
    struct HilChoice
    {
        const char* name;
        T value;
    };

    class HilArguments
    {
    public:
        static std::optional<uint32_t> ParseNumber(infra::BoundedConstString text);
        static std::optional<hal::DutyCycle> ParseDutyCycle(infra::BoundedConstString text);
        static std::optional<HilPinId> ParsePin(infra::BoundedConstString text, const HilPinNaming& naming);
        static HilStatus ParseHex(infra::BoundedConstString text, infra::ByteRange output, std::size_t& size);
        template<class T, std::size_t N>
        static std::optional<T> ParseChoice(infra::BoundedConstString text, const std::array<HilChoice<T>, N>& choices);

        explicit HilArguments(infra::BoundedConstString parameters);

        bool Shape(std::size_t minimumPositional, std::size_t maximumPositional, std::initializer_list<const char*> keys) const;
        bool Shape(std::size_t minimumPositional, std::size_t maximumPositional, infra::MemoryRange<const char* const> keys) const;
        std::size_t PositionalCount() const;
        infra::BoundedConstString Positional(std::size_t index) const;
        std::optional<infra::BoundedConstString> Key(const char* key) const;
        bool Has(const char* key) const;

        void NumberAt(std::size_t index, uint32_t& value, uint32_t minimum, uint32_t maximum, HilStatus& status) const;
        void Number(const char* key, uint32_t& value, uint32_t minimum, uint32_t maximum, HilStatus& status) const;
        void Flag(const char* key, bool& value, HilStatus& status) const;
        void Pin(const char* key, const HilPinNaming& naming, std::optional<HilPinId>& pin, HilStatus& status) const;
        void PinAt(std::size_t index, const HilPinNaming& naming, HilPinId& pin, HilStatus& status) const;
        void PinAt(std::size_t index, const HilPinNaming& naming, HilPinId& pin, HilPull& aliasPull, HilStatus& status) const;

        template<class T, std::size_t N>
        void Select(const char* key, T& value, const std::array<HilChoice<T>, N>& choices, HilStatus& status) const;
        template<class T, std::size_t N>
        void SelectAt(std::size_t index, T& value, const std::array<HilChoice<T>, N>& choices, HilStatus& status) const;

    private:
        template<class T, std::size_t N>
        static void Select(infra::BoundedConstString text, T& value, const std::array<HilChoice<T>, N>& choices, HilStatus& status);

        bool KnownKeys(infra::MemoryRange<const char* const> keys) const;
        static void Number(infra::BoundedConstString text, uint32_t& value, uint32_t minimum, uint32_t maximum, HilStatus& status);
        static bool IsKeyValue(infra::BoundedConstString token);

    private:
        infra::Tokenizer tokenizer;
        std::size_t tokens;
    };

    ////    Implementation    ////

    template<class T, std::size_t N>
    std::optional<T> HilArguments::ParseChoice(infra::BoundedConstString text, const std::array<HilChoice<T>, N>& choices)
    {
        for (const auto& choice : choices)
            if (text == choice.name)
                return choice.value;

        return std::nullopt;
    }

    template<class T, std::size_t N>
    void HilArguments::Select(const char* key, T& value, const std::array<HilChoice<T>, N>& choices, HilStatus& status) const
    {
        if (status != HilStatus::done)
            return;

        if (auto text = Key(key))
            Select(*text, value, choices, status);
    }

    template<class T, std::size_t N>
    void HilArguments::SelectAt(std::size_t index, T& value, const std::array<HilChoice<T>, N>& choices, HilStatus& status) const
    {
        if (status == HilStatus::done)
            Select(Positional(index), value, choices, status);
    }

    template<class T, std::size_t N>
    void HilArguments::Select(infra::BoundedConstString text, T& value, const std::array<HilChoice<T>, N>& choices, HilStatus& status)
    {
        if (auto choice = ParseChoice(text, choices))
            value = *choice;
        else
            status = HilStatus::usage;
    }
}

#endif
