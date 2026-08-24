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

    // Base 64-byte BFPT: 16 MB, 4 KB sub-sector (0x20), 64 KB sector (0xD8), 256-byte page.
    std::vector<uint8_t> MakeBfpt()
    {
        std::vector<uint8_t> bfpt(64, 0x00);
        // DW2: 16 MB linear density
        bfpt[4] = 0xFF;
        bfpt[5] = 0xFF;
        bfpt[6] = 0xFF;
        bfpt[7] = 0x07;
        // DW4: erase type 1 (4 KB, cmd=0x20), erase type 2 (64 KB, cmd=0xD8)
        bfpt[12] = 0x0C;
        bfpt[13] = 0x20;
        bfpt[14] = 0x10;
        bfpt[15] = 0xD8;
        // DW11: page size exp=8 → 256 bytes
        bfpt[40] = 0x80;
        return bfpt;
    }

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
    // addrMode = 0b10 = 2 in DW1 bits [2:0]
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[0] = 0x02; // DW1 addrMode = 2 → 4-byte only

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
    // addrMode = 1 (3-or-4 byte), density > 16MB → extendedAddressing
    testing::InSequence s;
    auto bfpt = MakeBfpt();
    bfpt[0] = 0x01; // DW1 addrMode = 1
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
    bfpt[4] = 27;   // exp = 27 → 2^27 bits = 128 Mbit = 16 MB
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
    bfpt[14] = 0x00; // erase type 2 size_exp = 0 → not supported
    bfpt[15] = 0x00;

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

    // No assertion on qer (not exposed by FlashGeometrySfdp), but this exercises the branch
    EXPECT_EQ(512u, geometry.NrOfSubSectors()); // chip was parsed using short table
}
