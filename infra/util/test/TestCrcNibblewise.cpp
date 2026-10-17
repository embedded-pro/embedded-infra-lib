#include "infra/util/ByteRange.hpp"
#include "infra/util/Crc.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>

namespace
{
    constexpr std::array<uint8_t, 9> checkInput = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };

    template<class Crc>
    auto CheckValue()
    {
        Crc crc;
        crc.Update(infra::MakeConstByteRange(checkInput));
        return crc.Result();
    }

    template<class Table, class Nibblewise>
    void ExpectEqualForEveryByte()
    {
        Table table;
        Nibblewise nibblewise;

        for (uint32_t byte = 0; byte != 256; ++byte)
        {
            table.Update(static_cast<uint8_t>(byte));
            nibblewise.Update(static_cast<uint8_t>(byte));
            EXPECT_EQ(table.Result(), nibblewise.Result());
        }
    }
}

TEST(CrcNibblewiseTest, computes_the_check_values_of_the_table_driven_crcs)
{
    EXPECT_EQ(0xa1, (CheckValue<infra::CrcNibblewise<uint8_t, 0x31, 0, 0, true, true>>()));
    EXPECT_EQ(0xf7, (CheckValue<infra::CrcNibblewise<uint8_t, 0x31, 0xff, 0, false, false>>()));
    EXPECT_EQ(0x4b37, (CheckValue<infra::CrcNibblewise<uint16_t, 0x8005, 0xffff, 0, true, true>>()));
    EXPECT_EQ(0x31c3, (CheckValue<infra::CrcNibblewise<uint16_t, 0x1021, 0x0>>()));
    EXPECT_EQ(0x29b1, (CheckValue<infra::CrcNibblewise<uint16_t, 0x1021, 0xffff>>()));
    EXPECT_EQ(0xcbf43926, CheckValue<infra::Crc32Nibblewise>());
    EXPECT_EQ(0x995dc9bbdf1939fa, (CheckValue<infra::CrcNibblewise<uint64_t, 0x42F0E1EBA9EA3693, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, true, true>>()));
}

TEST(CrcNibblewiseTest, equals_the_table_driven_crc_byte_by_byte)
{
    ExpectEqualForEveryByte<infra::Crc32, infra::Crc32Nibblewise>();
    ExpectEqualForEveryByte<infra::Crc16CcittFalse, infra::CrcNibblewise<uint16_t, 0x1021, 0xffff>>();
    ExpectEqualForEveryByte<infra::Crc8Sensirion, infra::CrcNibblewise<uint8_t, 0x31, 0xff, 0, false, false>>();
    ExpectEqualForEveryByte<infra::Crc8Maxim, infra::CrcNibblewise<uint8_t, 0x31, 0, 0, true, true>>();
}

TEST(CrcNibblewiseTest, reset_restores_the_initial_value)
{
    infra::Crc32Nibblewise crc;
    crc.Update(1);
    crc.Update(2);
    crc.Reset();

    EXPECT_EQ(0, crc.Result());
}
