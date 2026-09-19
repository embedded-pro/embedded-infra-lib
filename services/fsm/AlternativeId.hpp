#ifndef SERVICES_ALTERNATIVE_ID_HPP
#define SERVICES_ALTERNATIVE_ID_HPP

#include "infra/util/ReallyAssert.hpp"
#include <array>
#include <cstddef>
#include <type_traits>
#include <variant>

namespace services
{
    namespace detail
    {
        template<class Variant>
        struct AlternativeTraits;

        template<class... T>
        struct AlternativeTraits<std::variant<T...>>
        {
            static constexpr std::size_t count = sizeof...(T);
            static constexpr std::array<const char*, sizeof...(T)> names{ T::name... };

            template<class U>
            static constexpr std::size_t IndexOf()
            {
                constexpr std::array<bool, sizeof...(T)> matches{ std::is_same_v<U, T>... };
                std::size_t result = sizeof...(T);

                for (std::size_t i = 0; i != sizeof...(T); ++i)
                    if (matches[i] && result == sizeof...(T))
                        result = i;

                return result;
            }
        };
    }

    template<class Variant>
    class AlternativeId
    {
    public:
        static constexpr std::size_t count = detail::AlternativeTraits<Variant>::count;

        template<class T>
        static constexpr AlternativeId Of();
        static constexpr AlternativeId Of(const Variant& value);
        static constexpr AlternativeId FromIndex(std::size_t index);

        template<class T>
        constexpr bool Is() const;
        constexpr std::size_t Index() const;
        constexpr const char* Name() const;

        constexpr bool operator==(const AlternativeId& other) const = default;

    private:
        explicit constexpr AlternativeId(std::size_t index);

        std::size_t index;
    };

    ////    Implementation    ////

    template<class Variant>
    template<class T>
    constexpr AlternativeId<Variant> AlternativeId<Variant>::Of()
    {
        constexpr std::size_t index = detail::AlternativeTraits<Variant>::template IndexOf<T>();
        static_assert(index != count, "T is not an alternative of Variant");
        return AlternativeId{ index };
    }

    template<class Variant>
    constexpr AlternativeId<Variant> AlternativeId<Variant>::Of(const Variant& value)
    {
        really_assert(!value.valueless_by_exception());
        return AlternativeId{ value.index() };
    }

    template<class Variant>
    constexpr AlternativeId<Variant> AlternativeId<Variant>::FromIndex(std::size_t index)
    {
        really_assert(index < count);
        return AlternativeId{ index };
    }

    template<class Variant>
    template<class T>
    constexpr bool AlternativeId<Variant>::Is() const
    {
        return *this == Of<T>();
    }

    template<class Variant>
    constexpr std::size_t AlternativeId<Variant>::Index() const
    {
        return index;
    }

    template<class Variant>
    constexpr const char* AlternativeId<Variant>::Name() const
    {
        return detail::AlternativeTraits<Variant>::names[index];
    }

    template<class Variant>
    constexpr AlternativeId<Variant>::AlternativeId(std::size_t index)
        : index(index)
    {}
}

#endif
