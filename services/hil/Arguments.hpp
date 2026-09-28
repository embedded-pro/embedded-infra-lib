#ifndef SERVICES_HIL_ARGUMENTS_HPP
#define SERVICES_HIL_ARGUMENTS_HPP

#include "hal/interfaces/DutyCycle.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/Tokenizer.hpp"
#include "services/hil/PinId.hpp"
#include "services/hil/PinNaming.hpp"
#include "services/hil/Status.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>

namespace services::hil
{
    template<class T>
    struct Choice
    {
        const char* name;
        T value;
    };

    std::optional<uint32_t> ParseNumber(infra::BoundedConstString text);
    std::optional<hal::DutyCycle> ParseDutyCycle(infra::BoundedConstString text);
    std::optional<PinId> ParsePin(infra::BoundedConstString text, const PinNaming& naming);
    Status ParseHex(infra::BoundedConstString text, infra::ByteRange output, std::size_t& size);

    template<class T, std::size_t N>
    std::optional<T> ParseChoice(infra::BoundedConstString text, const std::array<Choice<T>, N>& choices)
    {
        for (const auto& choice : choices)
            if (text == choice.name)
                return choice.value;

        return std::nullopt;
    }

    class Arguments
    {
    public:
        explicit Arguments(infra::BoundedConstString parameters);

        bool Shape(std::size_t minimumPositional, std::size_t maximumPositional, std::initializer_list<const char*> keys) const;
        bool Shape(std::size_t minimumPositional, std::size_t maximumPositional, infra::MemoryRange<const char* const> keys) const;
        std::size_t PositionalCount() const;
        infra::BoundedConstString Positional(std::size_t index) const;
        std::optional<infra::BoundedConstString> Key(const char* key) const;
        bool Has(const char* key) const;

        void NumberAt(std::size_t index, uint32_t& value, uint32_t minimum, uint32_t maximum, Status& status) const;
        void Number(const char* key, uint32_t& value, uint32_t minimum, uint32_t maximum, Status& status) const;
        void Flag(const char* key, bool& value, Status& status) const;
        void Pin(const char* key, const PinNaming& naming, std::optional<PinId>& pin, Status& status) const;
        void PinAt(std::size_t index, const PinNaming& naming, PinId& pin, Status& status) const;
        void PinAt(std::size_t index, const PinNaming& naming, PinId& pin, Pull& aliasPull, Status& status) const;

        template<class T, std::size_t N>
        void Select(const char* key, T& value, const std::array<Choice<T>, N>& choices, Status& status) const;
        template<class T, std::size_t N>
        void SelectAt(std::size_t index, T& value, const std::array<Choice<T>, N>& choices, Status& status) const;

    private:
        template<class T, std::size_t N>
        static void Select(infra::BoundedConstString text, T& value, const std::array<Choice<T>, N>& choices, Status& status);

        template<class Keys>
        bool KnownKeys(const Keys& keys) const;
        static void Number(infra::BoundedConstString text, uint32_t& value, uint32_t minimum, uint32_t maximum, Status& status);
        static bool IsKeyValue(infra::BoundedConstString token);

    private:
        infra::Tokenizer tokenizer;
        std::size_t tokens;
    };

    ////    Implementation    ////

    template<class T, std::size_t N>
    void Arguments::Select(const char* key, T& value, const std::array<Choice<T>, N>& choices, Status& status) const
    {
        if (status != Status::done)
            return;

        if (auto text = Key(key))
            Select(*text, value, choices, status);
    }

    template<class T, std::size_t N>
    void Arguments::SelectAt(std::size_t index, T& value, const std::array<Choice<T>, N>& choices, Status& status) const
    {
        if (status == Status::done)
            Select(Positional(index), value, choices, status);
    }

    template<class T, std::size_t N>
    void Arguments::Select(infra::BoundedConstString text, T& value, const std::array<Choice<T>, N>& choices, Status& status)
    {
        if (auto choice = ParseChoice(text, choices))
            value = *choice;
        else
            status = Status::usage;
    }

    template<class Keys>
    bool Arguments::KnownKeys(const Keys& keys) const
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
}

#endif
