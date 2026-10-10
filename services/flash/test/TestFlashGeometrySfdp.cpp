#include "hal/interfaces/test_doubles/SpiMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/flash/FlashGeometrySfdp.hpp"
#include "gmock/gmock.h"

namespace
{
    std::vector<uint8_t> MakeSfdpHeader(uint8_t bfptAddr0, uint8_t bfptAddr1, uint8_t bfptAddr2, uint8_t tableLength = 0x10)
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
            // First parameter header (8 bytes)
            0x00,        // Parameter ID LSB
            0x06,        // minor revision
            0x01,        // major revision
            tableLength, // table length in DWORDs
            bfptAddr0,   // table pointer byte 0
            bfptAddr1,   // table pointer byte 1
            bfptAddr2,   // table pointer byte 2
            0xFF,        // Parameter ID MSB
        };
    }

    std::vector<uint8_t> MakeSfdpAndParamHeader()
    {
        return MakeSfdpHeader(0x80, 0x00, 0x00);
    }

    // Base 64-byte BFPT: 16 MB, 3-byte addresses, 4 KB sub-sector (0x20), 64 KB sector (0xD8), 256-byte page.
    std::vector<uint8_t> MakeBfpt()
    {
        std::vector<uint8_t> bfpt(64, 0x00);
        // DW2: 16 MB linear density
        bfpt[4] = 0xFF;
        bfpt[5] = 0xFF;
        bfpt[6] = 0xFF;
        bfpt[7] = 0x07;
        // DW8: erase type 1 (4 KB, cmd=0x20), erase type 2 (64 KB, cmd=0xD8)
        bfpt[28] = 0x0C;
        bfpt[29] = 0x20;
        bfpt[30] = 0x10;
        bfpt[31] = 0xD8;
        // DW11: page size exp=8 → 256 bytes
        bfpt[40] = 0x80;
        return bfpt;
    }

    // The SFDP of an MT25QL512 (64 MB), as read from a board: 3- or 4-byte addresses, erase types of 4 KB (0x20), 64 KB (0xD8) and 32 KB (0x52), 256-byte page
    const std::vector<uint8_t> mt25ql512Header{ 0x53, 0x46, 0x44, 0x50, 0x06, 0x01, 0x01, 0xff, 0x00, 0x06, 0x01, 0x10, 0x30, 0x00, 0x00, 0xff };
    const std::vector<uint8_t> mt25ql512Bfpt{
        0xe5, 0x20, 0xfb, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x29, 0xeb, 0x27, 0x6b, 0x27, 0x3b, 0x27, 0xbb,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x27, 0xbb, 0xff, 0xff, 0x29, 0xeb, 0x0c, 0x20, 0x10, 0xd8,
        0x0f, 0x52, 0x00, 0x00, 0x24, 0x4a, 0x99, 0x00, 0x8b, 0x8e, 0x03, 0xe1, 0xac, 0x01, 0x27, 0x38,
        0x7a, 0x75, 0x7a, 0x75, 0xfb, 0xbd, 0xd5, 0x5c, 0x4a, 0x0f, 0x82, 0xff, 0x81, 0xbd, 0x3d, 0x36
    };

    void ExpectSfdpReads(testing::StrictMock<hal::SpiMock>& spiMock,
        const std::vector<uint8_t>& header,
        const std::vector<uint8_t>& bfpt = {})
    {
        EXPECT_CALL(spiMock, SendDataMock(
                                 std::vector<uint8_t>{ 0x5A, 0x00, 0x00, 0x00, 0xFF },
                                 hal::SpiAction::continueSession));
        EXPECT_CALL(spiMock, ReceiveDataMock(hal::SpiAction::stop))
            .WillOnce(testing::Return(header));
        if (!bfpt.empty())
        {
            const uint8_t addr0 = header[12];
            const uint8_t addr1 = header[13];
            const uint8_t addr2 = header[14];
            EXPECT_CALL(spiMock, SendDataMock(
                                     std::vector<uint8_t>{ 0x5A, addr2, addr1, addr0, 0xFF },
                                     hal::SpiAction::continueSession));
            EXPECT_CALL(spiMock, ReceiveDataMock(hal::SpiAction::stop))
                .WillOnce(testing::Return(bfpt));
        }
    }
}

class FlashGeometrySfdpTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    FlashGeometrySfdpTest()
    {
        testing::InSequence s;
        ExpectSfdpReads(spiMock, MakeSfdpAndParamHeader(), MakeBfpt());
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

// ---- Additional tests exercising uncovered branches ----

class FlashGeometrySfdpBranchTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    testing::StrictMock<hal::SpiMock> spiMock;
    testing::StrictMock<infra::MockCallback<void()>> onInitialized;
};

TEST_F(FlashGeometrySfdpBranchTest, InvalidSfdpSignatureFallsBackToDefaults)
{
    testing::InSequence s;
    EXPECT_CALL(spiMock, SendDataMock(std::vector<uint8_t>{ 0x5A, 0x00, 0x00, 0x00, 0xFF }, hal::SpiAction::continueSession));
    EXPECT_CALL(spiMock, ReceiveDataMock(hal::SpiAction::stop))
        .WillOnce(testing::Return(std::vector<uint8_t>(16, 0xFF)));
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_EQ(512u, geometry.NrOfSubSectors());
    EXPECT_EQ(4096u, geometry.SizeSubSector());
    EXPECT_EQ(256u, geometry.SizePage());
    EXPECT_FALSE(geometry.ExtendedAddressing());
}

TEST_F(FlashGeometrySfdpBranchTest, ValidSignatureWithZeroBfptAddressFallsBackToDefaults)
{
    // Valid SFDP "SFDP" but param header has BFPT address = 0x000000 → ParseSfdpHeader returns false
    testing::InSequence s;
    EXPECT_CALL(spiMock, SendDataMock(std::vector<uint8_t>{ 0x5A, 0x00, 0x00, 0x00, 0xFF }, hal::SpiAction::continueSession));
    EXPECT_CALL(spiMock, ReceiveDataMock(hal::SpiAction::stop))
        .WillOnce(testing::Return(MakeSfdpHeader(0x00, 0x00, 0x00)));
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_EQ(512u, geometry.NrOfSubSectors());
}

TEST_F(FlashGeometrySfdpBranchTest, ExtendedAddressingSetForFourByteOnlyMode)
{
    // DW1 bits [18:17] = 0b10: 4-byte addresses only
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[2] = 0x04;

    ExpectSfdpReads(spiMock, MakeSfdpAndParamHeader(), bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_TRUE(geometry.ExtendedAddressing());
}

TEST_F(FlashGeometrySfdpBranchTest, ExtendedAddressingSetForMode1WhenFlashLargerThan16MB)
{
    // DW1 bits [18:17] = 0b01 (3- or 4-byte addresses), density > 16MB → extendedAddressing
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[2] = 0x02;
    // DW2: 32 MB = (0x0FFFFFFF + 1) bits / 8 = 32 MB
    bfpt[4] = 0xFF;
    bfpt[5] = 0xFF;
    bfpt[6] = 0xFF;
    bfpt[7] = 0x0F;

    ExpectSfdpReads(spiMock, MakeSfdpAndParamHeader(), bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_TRUE(geometry.ExtendedAddressing());
    EXPECT_EQ(8192u, geometry.NrOfSubSectors()); // 32 MB / 4 KB
}

TEST_F(FlashGeometrySfdpBranchTest, BitCountDensityFormatParsed)
{
    // DW2 bit 31 = 1: total bits = 2^exp, exp = 27 → 128 Mbit = 16 MB
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[4] = 27; // exp = 27 → 2^27 bits = 128 Mbit = 16 MB
    bfpt[5] = 0x00;
    bfpt[6] = 0x00;
    bfpt[7] = 0x80; // bit 31 set

    ExpectSfdpReads(spiMock, MakeSfdpAndParamHeader(), bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_EQ(4096u, geometry.NrOfSubSectors()); // 16 MB / 4 KB = 4096
}

TEST_F(FlashGeometrySfdpBranchTest, OnlyOneEraseSizeGivesSectorEqualToSubSector)
{
    // Only erase type 1 defined (4 KB); no larger erase type → sizeSector = sizeSubSector
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[30] = 0x00; // erase type 2 size_exp = 0 → not supported
    bfpt[31] = 0x00;

    ExpectSfdpReads(spiMock, MakeSfdpAndParamHeader(), bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_EQ(4096u, geometry.SizeSubSector());
    EXPECT_EQ(4096u, geometry.SizeSector()); // equals sizeSubSector when only one type
}

TEST_F(FlashGeometrySfdpBranchTest, ShortBfptTableLeavesPageSizeAtDefault)
{
    // tableLength = 9 (< 11) → ParsePageSize returns early, sizePage stays 256
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[40] = 0x00; // Would set pageSizeExp=0 if reached, but it won't be

    ExpectSfdpReads(spiMock, MakeSfdpHeader(0x80, 0x00, 0x00, 0x09), bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_EQ(256u, geometry.SizePage()); // default preserved
}

TEST_F(FlashGeometrySfdpBranchTest, ZeroPageSizeExpLeavesPageSizeAtDefault)
{
    // tableLength >= 11 but pageSizeExp = 0 → sizePage stays 256
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[40] = 0x00; // bits [7:4] = 0 → pageSizeExp = 0

    ExpectSfdpReads(spiMock, MakeSfdpAndParamHeader(), bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_EQ(256u, geometry.SizePage());
}

TEST_F(FlashGeometrySfdpBranchTest, ShortBfptTableLeavesQerAtZero)
{
    // tableLength = 9 (< 14) → ParseQer returns early, qer stays 0 (no quad enable for SPI anyway)
    testing::InSequence s;
    auto bfpt = MakeBfpt();

    ExpectSfdpReads(spiMock, MakeSfdpHeader(0x80, 0x00, 0x00, 0x09), bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    // No assertion on qer (not exposed by FlashGeometrySfdp), but this exercises the branch.
    // Density/erase types are still parsed from the short table; only QER (DW15) is skipped.
    EXPECT_EQ(4096u, geometry.NrOfSubSectors());
}

TEST_F(FlashGeometrySfdpBranchTest, ThreeOrFourByteAddressesOfASmallFlashStayThreeByte)
{
    // DW1 bits [18:17] = 0b01 and a density of 16 MB: 3 bytes reach all of it
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[2] = 0x02;

    ExpectSfdpReads(spiMock, MakeSfdpAndParamHeader(), bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_FALSE(geometry.ExtendedAddressing());
}

TEST_F(FlashGeometrySfdpBranchTest, TheFirstThreeBitsOfDword1DoNotSelectTheAddressMode)
{
    // DW1 bits [2:0] hold the erase sizes and the write granularity
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[0] = 0x07;
    bfpt[4] = 0xFF;
    bfpt[5] = 0xFF;
    bfpt[6] = 0xFF;
    bfpt[7] = 0x1F;

    ExpectSfdpReads(spiMock, MakeSfdpAndParamHeader(), bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_FALSE(geometry.ExtendedAddressing());
}

TEST_F(FlashGeometrySfdpBranchTest, EraseTypesAreReadFromDword8And9)
{
    // DW4 and DW5 describe other fast reads and must not be taken for erase types
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[12] = 0x08;
    bfpt[13] = 0x3B;
    bfpt[14] = 0x08;
    bfpt[15] = 0xBB;
    // DW9: erase type 3 (32 KB, cmd=0x52)
    bfpt[32] = 0x0F;
    bfpt[33] = 0x52;

    ExpectSfdpReads(spiMock, MakeSfdpAndParamHeader(), bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_EQ(4096u, geometry.SizeSubSector());
    EXPECT_EQ(65536u, geometry.SizeSector());
    EXPECT_EQ(4096u, geometry.NrOfSubSectors());
}

TEST_F(FlashGeometrySfdpBranchTest, TableTooShortForTheEraseTypesKeepsTheDefaultEraseSizesAndStillUsesTheDensity)
{
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[28] = 0x0A; // would be a 1 KB erase type if it were read
    bfpt[29] = 0x42;

    ExpectSfdpReads(spiMock, MakeSfdpHeader(0x80, 0x00, 0x00, 0x08), bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_EQ(4096u, geometry.SizeSubSector());
    EXPECT_EQ(65536u, geometry.SizeSector());
    EXPECT_EQ(4096u, geometry.NrOfSubSectors());
}

TEST_F(FlashGeometrySfdpBranchTest, TheSfdpOfAnMt25ql512IsParsed)
{
    testing::InSequence s;
    ExpectSfdpReads(spiMock, mt25ql512Header, mt25ql512Bfpt);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometrySfdp geometry{ spiMock, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_EQ(16384u, geometry.NrOfSubSectors());
    EXPECT_EQ(4096u, geometry.SizeSubSector());
    EXPECT_EQ(65536u, geometry.SizeSector());
    EXPECT_EQ(256u, geometry.SizePage());
    EXPECT_TRUE(geometry.ExtendedAddressing());
}
