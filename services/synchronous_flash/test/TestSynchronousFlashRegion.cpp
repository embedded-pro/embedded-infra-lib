#include "hal/synchronous_interfaces/test_doubles/SynchronousFlashStub.hpp"
#include "services/synchronous_flash/SynchronousFlashRegion.hpp"
#include "gtest/gtest.h"

class SynchronousFlashRegionTest
    : public testing::Test
{
public:
    SynchronousFlashRegionTest()
        : region(master, 2, 4)
    {}

    hal::SynchronousFlashStub master{ 8, 4096 };
    services::SynchronousFlashRegion region;
};

TEST_F(SynchronousFlashRegionTest, NumberOfSectors)
{
    EXPECT_EQ(4, region.NumberOfSectors());
}

TEST_F(SynchronousFlashRegionTest, SizeOfSector)
{
    EXPECT_EQ(4096, region.SizeOfSector(0));
    EXPECT_EQ(4096, region.SizeOfSector(1));
}

TEST_F(SynchronousFlashRegionTest, SectorOfAddress)
{
    EXPECT_EQ(0, region.SectorOfAddress(0));
    EXPECT_EQ(0, region.SectorOfAddress(100));
    EXPECT_EQ(1, region.SectorOfAddress(4096));
}

TEST_F(SynchronousFlashRegionTest, AddressOfSector)
{
    EXPECT_EQ(0, region.AddressOfSector(0));
    EXPECT_EQ(4096, region.AddressOfSector(1));
    EXPECT_EQ(8192, region.AddressOfSector(2));
}

TEST_F(SynchronousFlashRegionTest, WriteBuffer)
{
    std::array<uint8_t, 4> data = { 0x12, 0x34, 0x56, 0x78 };
    region.WriteBuffer(data, 0);

    std::array<uint8_t, 4> readBack{};
    master.ReadBuffer(readBack, 2 * 4096);
    EXPECT_EQ(data, readBack);
}

TEST_F(SynchronousFlashRegionTest, ReadBuffer)
{
    std::array<uint8_t, 4> data = { 0xAA, 0xBB, 0xCC, 0xDD };
    master.WriteBuffer(data, 2 * 4096 + 100);

    std::array<uint8_t, 4> readBack{};
    region.ReadBuffer(readBack, 100);
    EXPECT_EQ(data, readBack);
}

TEST_F(SynchronousFlashRegionTest, EraseSectors)
{
    std::array<uint8_t, 4> data = { 0x12, 0x34, 0x56, 0x78 };
    region.WriteBuffer(data, 0);
    region.WriteBuffer(data, 4096);

    region.EraseSectors(0, 2);

    std::array<uint8_t, 4> readBack{};
    region.ReadBuffer(readBack, 0);
    EXPECT_EQ((std::array<uint8_t, 4>{ 0xFF, 0xFF, 0xFF, 0xFF }), readBack);

    region.ReadBuffer(readBack, 4096);
    EXPECT_EQ((std::array<uint8_t, 4>{ 0xFF, 0xFF, 0xFF, 0xFF }), readBack);
}
