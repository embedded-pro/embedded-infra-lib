#ifndef INFRA_ENDIAN_HPP
#define INFRA_ENDIAN_HPP

#include "infra/util/ByteRange.hpp"
#include <algorithm>
#include <bit>
#include <climits>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <utility>

namespace infra
{
    namespace detail
    {
#ifdef __cpp_lib_byteswap
        template<std::integral T>
        constexpr T ByteSwap(T value)
        {
            return std::byteswap(value);
        }
#else
        constexpr auto mask = static_cast<unsigned char>(-1);

        template<class T, std::size_t... N>
        constexpr T ByteSwapImpl(T i, std::index_sequence<N...>)
        {
            return ((((i >> (N * CHAR_BIT)) & static_cast<T>(mask)) << ((sizeof(T) - 1 - N) * CHAR_BIT)) | ...);
        }

        template<class T, class U = typename std::make_unsigned<T>::type>
        constexpr U ByteSwap(T i)
        {
            return ByteSwapImpl<U>(i, std::make_index_sequence<sizeof(T)>{});
        }
#endif
    }

    constexpr bool isLittleEndian = std::endian::native == std::endian::little;
    constexpr bool isBigEndian = std::endian::native == std::endian::big;

    template<class T>
    constexpr T SwapEndian(T value)
    {
        return detail::ByteSwap(value);
    }

    template<class T, std::size_t N>
    std::array<T, N> SwapEndian(std::array<T, N> value)
    {
        std::array<T, N> result;
        auto valueRange = infra::MakeByteRange(value);
        std::copy(valueRange.begin(), valueRange.end(), std::make_reverse_iterator(infra::MakeByteRange(result).end()));
        return result;
    }

    template<class T>
    constexpr T FromBigEndian(T value)
    {
        if constexpr (isLittleEndian)
            return SwapEndian(value);
        else if constexpr (isBigEndian)
            return value;
    }

    template<class T>
    constexpr T ToBigEndian(T value)
    {
        if constexpr (isLittleEndian)
            return SwapEndian(value);
        else if constexpr (isBigEndian)
            return value;
    }

    template<class T>
    constexpr T FromLittleEndian(T value)
    {
        if constexpr (isLittleEndian)
            return value;
        else if constexpr (isBigEndian)
            return SwapEndian(value);
    }

    template<class T>
    constexpr T ToLittleEndian(T value)
    {
        if constexpr (isLittleEndian)
            return value;
        else if constexpr (isBigEndian)
            return SwapEndian(value);
    }

    template<class T>
    class BigEndian
    {
    public:
        BigEndian() = default;

        constexpr BigEndian(T value)
            : value(ToBigEndian(value))
        {}

        constexpr operator T() const
        {
            return FromBigEndian(value);
        }

        bool operator==(BigEndian other) const
        {
            return value == other.value;
        }

        bool operator!=(BigEndian other) const
        {
            return !(*this == other);
        }

        bool operator==(T other) const
        {
            return value == ToBigEndian(other);
        }

        bool operator!=(T other) const
        {
            return !(*this == other);
        }

        template<class U>
        bool operator==(U other) const
        {
            return value == ToBigEndian(static_cast<T>(other));
        }

        template<class U>
        bool operator!=(U other) const
        {
            return !(*this == other);
        }

        friend bool operator==(T x, BigEndian y)
        {
            return y.operator==(x);
        }

        friend bool operator!=(T x, BigEndian y)
        {
            return y.operator!=(x);
        }

        template<class U>
        friend bool operator==(U x, BigEndian y)
        {
            return y.operator==(x);
        }

        template<class U>
        friend bool operator!=(U x, BigEndian y)
        {
            return y.operator!=(x);
        }

    private:
        T value{};
    };

    template<class T>
    class LittleEndian
    {
    public:
        LittleEndian() = default;

        constexpr LittleEndian(T value)
            : value(ToLittleEndian(value))
        {}

        constexpr operator T() const
        {
            return FromLittleEndian(value);
        }

        bool operator==(LittleEndian other) const
        {
            return value == other.value;
        }

        bool operator!=(LittleEndian other) const
        {
            return !(*this == other);
        }

        bool operator==(T other) const
        {
            return value == ToLittleEndian(other);
        }

        bool operator!=(T other) const
        {
            return !(*this == other);
        }

        template<class U>
        bool operator==(U other) const
        {
            return value == ToLittleEndian(static_cast<T>(other));
        }

        template<class U>
        bool operator!=(U other) const
        {
            return !(*this == other);
        }

        friend bool operator==(T x, LittleEndian y)
        {
            return y == x;
        }

        friend bool operator!=(T x, LittleEndian y)
        {
            return y != x;
        }

        template<class U>
        friend bool operator==(U x, LittleEndian y)
        {
            return y == x;
        }

        template<class U>
        friend bool operator!=(U x, LittleEndian y)
        {
            return y != x;
        }

    private:
        T value{};
    };
}

#endif
