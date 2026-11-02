#include "services/synchronous_flash/SynchronousFlashMemory.hpp"
#include <array>
#include <gmock/gmock.h>

namespace
{
    class TestSynchronousFlashMemory
        : public testing::Test
    {
    public:
        std::array<uint8_t, 64> memory{};
        services::SynchronousFlashMemory flash{ infra::MakeRange(memory), 16 };

        std::array<uint8_t, 4> Read(uint32_t address)
        {
            std::array<uint8_t, 4> bytes{};
            flash.ReadBuffer(infra::MakeRange(bytes), address);
            return bytes;
        }
    };
}

TEST_F(TestSynchronousFlashMemory, the_memory_is_divided_into_sectors)
{
    EXPECT_EQ(4u, flash.NumberOfSectors());
    EXPECT_EQ(16u, flash.SizeOfSector(2));
    EXPECT_EQ(2u, flash.SectorOfAddress(33));
    EXPECT_EQ(48u, flash.AddressOfSector(3));
}

TEST_F(TestSynchronousFlashMemory, reads_what_the_memory_holds)
{
    memory[20] = 1;
    memory[21] = 2;
    memory[22] = 3;
    memory[23] = 4;

    EXPECT_EQ((std::array<uint8_t, 4>{ 1, 2, 3, 4 }), Read(20));
}

TEST_F(TestSynchronousFlashMemory, an_erase_sets_its_sectors_and_only_them_to_0xff)
{
    flash.EraseSectors(1, 3);

    EXPECT_EQ((std::array<uint8_t, 4>{ 0, 0, 0, 0 }), Read(12));
    EXPECT_EQ((std::array<uint8_t, 4>{ 0xff, 0xff, 0xff, 0xff }), Read(16));
    EXPECT_EQ((std::array<uint8_t, 4>{ 0xff, 0xff, 0xff, 0xff }), Read(44));
    EXPECT_EQ((std::array<uint8_t, 4>{ 0, 0, 0, 0 }), Read(48));
}

TEST_F(TestSynchronousFlashMemory, a_write_clears_bits_as_flash_programming_does)
{
    flash.EraseSectors(0, 1);
    const std::array<uint8_t, 2> first{ 0xf0, 0x3c };
    const std::array<uint8_t, 2> second{ 0x0f, 0xff };

    flash.WriteBuffer(infra::MakeRange(first), 5);
    flash.WriteBuffer(infra::MakeRange(second), 5);

    EXPECT_EQ((std::array<uint8_t, 4>{ 0x00, 0x3c, 0xff, 0xff }), Read(5));
}
