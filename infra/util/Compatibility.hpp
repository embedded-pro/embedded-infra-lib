#ifndef INFRA_COMPATIBILITY_HPP
#define INFRA_COMPATIBILITY_HPP

#include <limits>
#include <type_traits>
#include <utility>

// This file contains compatibility wrappers for features that
// are not yet present in all implementations of the C++20
// standard library.

namespace infra
{
    static_assert(__cplusplus >= 202002L, "C++20 or later is required. When using MSVC, ensure the /Zc:__cplusplus flag is set.");

#ifdef __cpp_lib_integer_comparison_functions
    using std::in_range;
#else
    template<class T, class U>
    constexpr bool cmp_less(T t, U u) noexcept
    {
        if constexpr (std::is_signed_v<T> == std::is_signed_v<U>)
            return t < u;
        else if constexpr (std::is_signed_v<T>)
            return t < 0 || std::make_unsigned_t<T>(t) < u;
        else
            return u >= 0 && t < std::make_unsigned_t<U>(u);
    }

    template<class T, class U>
    constexpr bool cmp_less_equal(T t, U u) noexcept
    {
        return !cmp_less(u, t);
    }

    template<class T, class U>
    constexpr bool cmp_greater_equal(T t, U u) noexcept
    {
        return !cmp_less(t, u);
    }

    template<class R, class T>
    constexpr bool in_range(T t) noexcept
    {
        return cmp_greater_equal(t, std::numeric_limits<R>::min()) &&
               cmp_less_equal(t, std::numeric_limits<R>::max());
    }
#endif
}

#endif
