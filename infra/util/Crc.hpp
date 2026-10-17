#ifndef INFRA_CRC_HPP
#define INFRA_CRC_HPP

#include "infra/util/ByteRange.hpp"
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>

namespace infra
{
    template<typename CRC_TYPE>
    constexpr CRC_TYPE PolyReverse(CRC_TYPE Polynomial);

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue = 0, CRC_TYPE FinalXor = 0, bool ReflectInput = false, bool ReflectOutput = false>
    class Crc
    {
        static_assert(ReflectInput == ReflectOutput, "Currently, non-matching ReflectInput and ReflectOutput is not supported");

    public:
        void Update(uint8_t input);
        void Update(ConstByteRange bytes);
        CRC_TYPE Result() const;
        void Reset();

    private:
        class Table
        {
        public:
            constexpr Table();
            CRC_TYPE operator[](int i) const;

        private:
            CRC_TYPE table[256];
        };

        template<typename TYPE>
        static constexpr TYPE Reflect(TYPE value);

        static constexpr uint8_t bitWidth = sizeof(CRC_TYPE) * CHAR_BIT;
        static constexpr bool bitWidthEquals8 = bitWidth == 8;
        static constexpr uint8_t shift = bitWidth - 8;
        static inline constexpr Table table{};

        CRC_TYPE crc = InitValue;
    };

    namespace detail
    {
        template<typename TYPE>
        constexpr TYPE ReversedBits(TYPE value);
    }

    // The CRC of Crc without its table of 256 entries, for code that is bound by its size rather than by its speed
    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue = 0, CRC_TYPE FinalXor = 0, bool ReflectInput = false, bool ReflectOutput = false>
    class CrcBitwise
    {
        static_assert(ReflectInput == ReflectOutput, "Currently, non-matching ReflectInput and ReflectOutput is not supported");

    public:
        void Update(uint8_t input);
        void Update(ConstByteRange bytes);
        CRC_TYPE Result() const;
        void Reset();

    private:
        static constexpr uint8_t bitWidth = sizeof(CRC_TYPE) * CHAR_BIT;
        static constexpr CRC_TYPE topBit = static_cast<CRC_TYPE>(CRC_TYPE(1) << (bitWidth - 1));
        static constexpr CRC_TYPE reversedPolynomial = detail::ReversedBits(Polynomial);

        CRC_TYPE crc = InitValue;
    };

    // The CRC of Crc with a table of 16 entries, which takes two lookups per byte, for code that is bound by its size more than by its speed
    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue = 0, CRC_TYPE FinalXor = 0, bool ReflectInput = false, bool ReflectOutput = false>
    class CrcNibblewise
    {
        static_assert(ReflectInput == ReflectOutput, "Currently, non-matching ReflectInput and ReflectOutput is not supported");

    public:
        void Update(uint8_t input);
        void Update(ConstByteRange bytes);
        CRC_TYPE Result() const;
        void Reset();

    private:
        static constexpr std::array<CRC_TYPE, 16> MakeTable();

        static constexpr uint8_t bitWidth = sizeof(CRC_TYPE) * CHAR_BIT;
        static constexpr CRC_TYPE topBit = static_cast<CRC_TYPE>(CRC_TYPE(1) << (bitWidth - 1));
        static constexpr CRC_TYPE reversedPolynomial = detail::ReversedBits(Polynomial);
        static inline constexpr std::array<CRC_TYPE, 16> table = MakeTable();

        CRC_TYPE crc = InitValue;
    };

    using Crc8Maxim = Crc<uint8_t, 0x31, 0, 0, true, true>;
    using Crc8Sensirion = Crc<uint8_t, 0x31, 0xff, 0, false, false>;
    using Crc16Modbus = Crc<uint16_t, 0x8005, 0xffff, 0, true, true>;
    using Crc16Xmodem = Crc<uint16_t, 0x1021, 0x0>;
    using Crc16CcittFalse = Crc<uint16_t, 0x1021, 0xffff>;
    using Crc32 = Crc<uint32_t, 0x04C11DB7, 0xffffffff, 0xffffffff, true, true>;
    using Crc64Ecma = Crc<uint64_t, 0x42F0E1EBA9EA3693, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, true, true>;

    using Crc16CcittFalseBitwise = CrcBitwise<uint16_t, 0x1021, 0xffff>;

    using Crc32Nibblewise = CrcNibblewise<uint32_t, 0x04C11DB7, 0xffffffff, 0xffffffff, true, true>;

    namespace detail
    {
        template<typename TYPE>
        constexpr TYPE ReversedBits(TYPE value)
        {
            TYPE result = 0;

            for (std::size_t bit = 0; bit < sizeof(TYPE) * CHAR_BIT; ++bit, value >>= 1)
                result = static_cast<TYPE>((result << 1) | (value & 1));

            return result;
        }
    }

    template<typename CRC_TYPE>
    constexpr CRC_TYPE PolyReverse(CRC_TYPE Polynomial)
    {
        CRC_TYPE ret = 0;
        CRC_TYPE maskIn = 1;
        CRC_TYPE maskOut = 1 << (sizeof(CRC_TYPE) * 8 - 1);
        while (maskIn)
        {
            if (Polynomial & maskIn)
                ret |= maskOut;
            maskOut >>= 1;
            maskIn <<= 1;
        }
        return ret;
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    void Crc<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Update(uint8_t input)
    {
        if constexpr (ReflectInput)
        {
            crc ^= input;
            uint8_t pos = crc & 0xFF;
            if constexpr (bitWidthEquals8)
                crc = 0;
            else
                crc >>= 8;
            crc ^= table[pos];
        }
        else
        {
            crc ^= (CRC_TYPE(input) << shift);
            uint8_t pos = static_cast<uint8_t>(crc >> shift);
            if constexpr (bitWidthEquals8)
                crc = 0;
            else
                crc <<= 8;
            crc ^= table[pos];
        }
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    void Crc<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Update(ConstByteRange bytes)
    {
        for (auto byte : bytes)
            Update(byte);
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    CRC_TYPE Crc<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Result() const
    {
        return crc ^ FinalXor;
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    void Crc<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Reset()
    {
        crc = InitValue;
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    template<typename TYPE>
    constexpr TYPE Crc<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Reflect(TYPE value)
    {
        TYPE result = 0;
        constexpr size_t bitsToReflect = sizeof(TYPE) * CHAR_BIT;

        for (size_t i = 0; i < bitsToReflect; ++i, value >>= 1)
            result = (result << 1) | (value & 0x01);

        return result;
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    constexpr Crc<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Table::Table()
        : table()
    {
        for (unsigned int i = 0; i < 256; ++i)
        {
            auto input = static_cast<uint8_t>(i);
            if constexpr (ReflectInput)
                input = Reflect(input);

            CRC_TYPE crcEntry = static_cast<CRC_TYPE>(input) << shift;

            for (int bit = 0; bit < CHAR_BIT; ++bit)
            {
                constexpr CRC_TYPE msb = CRC_TYPE(1) << (bitWidth - 1);
                if ((crcEntry & msb) != 0)
                {
                    crcEntry <<= 1;
                    crcEntry ^= Polynomial;
                }
                else
                {
                    crcEntry <<= 1;
                }
            }

            if constexpr (ReflectOutput)
                table[i] = Reflect(crcEntry);
            else
                table[i] = crcEntry;
        }
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    CRC_TYPE Crc<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Table::operator[](int i) const
    {
        return table[i];
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    void CrcBitwise<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Update(uint8_t input)
    {
        if constexpr (ReflectInput)
        {
            crc ^= input;

            for (int bit = 0; bit < CHAR_BIT; ++bit)
                crc = (crc & 1) != 0 ? static_cast<CRC_TYPE>((crc >> 1) ^ reversedPolynomial) : static_cast<CRC_TYPE>(crc >> 1);
        }
        else
        {
            crc ^= static_cast<CRC_TYPE>(CRC_TYPE(input) << (bitWidth - CHAR_BIT));

            for (int bit = 0; bit < CHAR_BIT; ++bit)
                crc = (crc & topBit) != 0 ? static_cast<CRC_TYPE>((crc << 1) ^ Polynomial) : static_cast<CRC_TYPE>(crc << 1);
        }
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    void CrcBitwise<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Update(ConstByteRange bytes)
    {
        for (auto byte : bytes)
            Update(byte);
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    CRC_TYPE CrcBitwise<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Result() const
    {
        return crc ^ FinalXor;
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    void CrcBitwise<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Reset()
    {
        crc = InitValue;
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    constexpr std::array<CRC_TYPE, 16> CrcNibblewise<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::MakeTable()
    {
        std::array<CRC_TYPE, 16> result{};

        for (std::size_t nibble = 0; nibble != result.size(); ++nibble)
        {
            if constexpr (ReflectInput)
            {
                auto entry = static_cast<CRC_TYPE>(nibble);

                for (int bit = 0; bit < 4; ++bit)
                    entry = (entry & 1) != 0 ? static_cast<CRC_TYPE>((entry >> 1) ^ reversedPolynomial) : static_cast<CRC_TYPE>(entry >> 1);

                result[nibble] = entry;
            }
            else
            {
                auto entry = static_cast<CRC_TYPE>(CRC_TYPE(nibble) << (bitWidth - 4));

                for (int bit = 0; bit < 4; ++bit)
                    entry = (entry & topBit) != 0 ? static_cast<CRC_TYPE>((entry << 1) ^ Polynomial) : static_cast<CRC_TYPE>(entry << 1);

                result[nibble] = entry;
            }
        }

        return result;
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    void CrcNibblewise<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Update(uint8_t input)
    {
        if constexpr (ReflectInput)
        {
            crc = static_cast<CRC_TYPE>((crc >> 4) ^ table[(crc ^ input) & 0xf]);
            crc = static_cast<CRC_TYPE>((crc >> 4) ^ table[(crc ^ (input >> 4)) & 0xf]);
        }
        else
        {
            crc = static_cast<CRC_TYPE>((crc << 4) ^ table[((crc >> (bitWidth - 4)) ^ (input >> 4)) & 0xf]);
            crc = static_cast<CRC_TYPE>((crc << 4) ^ table[((crc >> (bitWidth - 4)) ^ input) & 0xf]);
        }
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    void CrcNibblewise<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Update(ConstByteRange bytes)
    {
        for (auto byte : bytes)
            Update(byte);
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    CRC_TYPE CrcNibblewise<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Result() const
    {
        return crc ^ FinalXor;
    }

    template<typename CRC_TYPE, CRC_TYPE Polynomial, CRC_TYPE InitValue, CRC_TYPE FinalXor, bool ReflectInput, bool ReflectOutput>
    void CrcNibblewise<CRC_TYPE, Polynomial, InitValue, FinalXor, ReflectInput, ReflectOutput>::Reset()
    {
        crc = InitValue;
    }
}

#endif
