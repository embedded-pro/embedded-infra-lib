#include "hal/interfaces/test_doubles/SpiMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/flash/FlashGeometrySfdp.hpp"
#include "gmock/gmock.h"

namespace
{
    // Returns 16 bytes: 8-byte SFDP header + 8-byte first parameter header
    // BFPT table at address 0x000080, 16 DWORDs long
    std::vector<uint8_t> MakeSfdpAndParamHeader()
    {
        return {
            // SFDP header (8 bytes)
            0x53, // 'S'
            0x46, // 'F'
            0x44, // 'D'
            0x50, // 'P'
            0x06, // minor revision
            0x01, // major revision
            0x00, // NPH = 1 parameter header total
            0xFF, // access protocol
            // First parameter header (8 bytes): JEDEC BFPT, ID=0xFF00
            0x00, // Parameter ID LSB
            0x06, // minor revision
            0x01, // major revision
            0x10, // table length = 16 DWORDs
            0x80, // table pointer byte 0
            0x00, // table pointer byte 1
            0x00, // table pointer byte 2  → address 0x000080
            0xFF, // Parameter ID MSB
        };
    }

    // Returns 64 bytes of BFPT for a 16MB chip with 4KB/64KB erase, 256-byte pages
    std::vector<uint8_t> MakeBfpt()
    {
        std::vector<uint8_t> bfpt(64, 0x00);
        // DW1 [0-3]: 3-byte addressing only (bits[2:0]=0)
        // DW2 [4-7]: 16MB density = (0x07FFFFFF+1) bits / 8 = 16MB
        bfpt[4] = 0xFF;
        bfpt[5] = 0xFF;
        bfpt[6] = 0xFF;
        bfpt[7] = 0x07;
        // DW3 [8-11]: no fast-read quad entry
        // DW4 [12-15]: erase type 1 (4096, 0x20) + erase type 2 (65536, 0xD8)
        bfpt[12] = 0x0C;
        bfpt[13] = 0x20;
        bfpt[14] = 0x10;
        bfpt[15] = 0xD8;
        // DW11 [40-43]: page size bits[7:4]=8 → 2^8=256 bytes
        bfpt[40] = 0x80;
        return bfpt;
    }
}

class FlashGeometrySfdpTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    FlashGeometrySfdpTest()
    {
        // geometry is already constructed (EventDispatcher.Schedule defers SFDP start)
        // Set up expectations before triggering the event loop; InSequence enforces order

        testing::InSequence s;

        EXPECT_CALL(spiMock, SendDataMock(
                                 std::vector<uint8_t>{ 0x5A, 0x00, 0x00, 0x00, 0xFF },
                                 hal::SpiAction::continueSession));
        EXPECT_CALL(spiMock, ReceiveDataMock(hal::SpiAction::stop))
            .WillOnce(testing::Return(MakeSfdpAndParamHeader()));

        EXPECT_CALL(spiMock, SendDataMock(
                                 std::vector<uint8_t>{ 0x5A, 0x00, 0x00, 0x80, 0xFF },
                                 hal::SpiAction::continueSession));
        EXPECT_CALL(spiMock, ReceiveDataMock(hal::SpiAction::stop))
            .WillOnce(testing::Return(MakeBfpt()));

        EXPECT_CALL(onInitialized, callback());

        ExecuteAllActions();
    }

    testing::StrictMock<hal::SpiMock> spiMock;
    testing::StrictMock<infra::MockCallback<void()>> onInitialized;
    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
};

TEST_F(FlashGeometrySfdpTest, NrOfSubSectorsDeducedFromDensityAndSmallestEraseType)
{
    // 16MB / 4KB = 4096 sub-sectors
    EXPECT_EQ(4096u, geometry.NrOfSubSectors());
}

TEST_F(FlashGeometrySfdpTest, SizeSubSectorIsSmallestEraseType)
{
    EXPECT_EQ(4096u, geometry.SizeSubSector());
}

TEST_F(FlashGeometrySfdpTest, SizeSectorIsLargestEraseType)
{
    EXPECT_EQ(65536u, geometry.SizeSector());
}

TEST_F(FlashGeometrySfdpTest, SizePageDeducedFromDword11)
{
    EXPECT_EQ(256u, geometry.SizePage());
}

TEST_F(FlashGeometrySfdpTest, ExtendedAddressingFalseFor3ByteOnlyChip)
{
    EXPECT_FALSE(geometry.ExtendedAddressing());
}
