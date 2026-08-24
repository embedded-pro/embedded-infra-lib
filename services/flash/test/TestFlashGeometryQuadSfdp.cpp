#include "hal/interfaces/test_doubles/QuadSpiStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/flash/FlashGeometryQuadSfdp.hpp"
#include "gmock/gmock.h"

namespace
{
    // 16 bytes: SFDP header (8) + first parameter header (8), BFPT at 0x000080.
    const std::vector<uint8_t> sfdpHeader = {
        0x53,
        0x46,
        0x44,
        0x50,
        0x06,
        0x01,
        0x00,
        0xFF,
        0x00,
        0x06,
        0x01,
        0x10,
        0x80,
        0x00,
        0x00,
        0xFF,
    };

    // 64-byte BFPT: 16 MB chip, 4 KB sub-sector, 64 KB sector, 256-byte page.
    // DW15 bits [22:20] hold the QER value.
    std::vector<uint8_t> MakeBfptWithQer(uint8_t qer)
    {
        std::vector<uint8_t> bfpt(64, 0x00);
        bfpt[4] = 0xFF;
        bfpt[5] = 0xFF;
        bfpt[6] = 0xFF;
        bfpt[7] = 0x07; // DW2: 16 MB
        bfpt[12] = 0x0C;
        bfpt[13] = 0x20;
        bfpt[14] = 0x10;
        bfpt[15] = 0xD8;                                    // DW4: erase types
        bfpt[40] = 0x80;                                    // DW11: 256-byte page
        bfpt[58] = static_cast<uint8_t>((qer & 0x07) << 4); // DW15: QER
        return bfpt;
    }

    hal::QuadSpi::Header SfdpHeader(uint32_t address)
    {
        return hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x5A }), hal::QuadSpi::AddressToVector(address, 3), {}, 8 };
    }
}

class FlashGeometryQuadSfdpTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    void ExpectSfdpRead(uint8_t qer)
    {
        std::vector<uint8_t> bfpt = MakeBfptWithQer(qer);

        EXPECT_CALL(spiStub, ReceiveDataMock(SfdpHeader(0x000000), hal::QuadSpi::Lines::SingleSpeed()))
            .WillOnce(testing::Return(infra::MakeRange(sfdpHeader.data(), sfdpHeader.data() + sfdpHeader.size())));

        EXPECT_CALL(spiStub, ReceiveDataMock(SfdpHeader(0x000080), hal::QuadSpi::Lines::SingleSpeed()))
            .WillOnce([bfpt](const hal::QuadSpi::Header&, hal::QuadSpi::Lines) -> infra::ConstByteRange
                {
                    static std::vector<uint8_t> storage;
                    storage = bfpt;
                    return infra::MakeRange(storage.data(), storage.data() + storage.size());
                });
    }

    testing::StrictMock<hal::QuadSpiStub> spiStub;
    testing::StrictMock<infra::MockCallback<void()>> onInitialized;
};

TEST_F(FlashGeometryQuadSfdpTest, GeometryParsedCorrectlyFromSfdp)
{
    testing::InSequence s;
    ExpectSfdpRead(0);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometryQuadSfdp geometry{ spiStub, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_EQ(4096u, geometry.NrOfSubSectors());
    EXPECT_EQ(4096u, geometry.SizeSubSector());
    EXPECT_EQ(65536u, geometry.SizeSector());
    EXPECT_EQ(256u, geometry.SizePage());
    EXPECT_FALSE(geometry.ExtendedAddressing());
    EXPECT_EQ(0x20, geometry.EraseSubSectorCommand());
    EXPECT_EQ(0xD8, geometry.EraseSectorCommand());
    EXPECT_EQ(0xC7, geometry.EraseBulkCommand());
    EXPECT_EQ(0x32, geometry.PageProgramCommand());
}

TEST_F(FlashGeometryQuadSfdpTest, Qer0DoesNotIssueExtraQuadEnableSpiCalls)
{
    testing::InSequence s;
    ExpectSfdpRead(0);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometryQuadSfdp geometry{ spiStub, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();
}

TEST_F(FlashGeometryQuadSfdpTest, Qer1EnablesQuadViaSr2Sr1WriteEnable)
{
    static uint8_t sr2Val = 0x00;
    static uint8_t sr1Val = 0x00;

    testing::InSequence s;
    ExpectSfdpRead(1);
    EXPECT_CALL(spiStub, ReceiveDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x35 }), {}, {}, 0 },
                             hal::QuadSpi::Lines::SingleSpeed()))
        .WillOnce(testing::Return(infra::MakeByteRange(sr2Val)));
    EXPECT_CALL(spiStub, ReceiveDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x05 }), {}, {}, 0 },
                             hal::QuadSpi::Lines::SingleSpeed()))
        .WillOnce(testing::Return(infra::MakeByteRange(sr1Val)));
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x06 }), {}, {}, 0 }, infra::ConstByteRange{}, hal::QuadSpi::Lines::SingleSpeed()));
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x01 }), {}, {}, 0 }, testing::_, hal::QuadSpi::Lines::SingleSpeed()));
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometryQuadSfdp geometry{ spiStub, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();
}

TEST_F(FlashGeometryQuadSfdpTest, Qer2EnablesQuadViaSr1WriteEnable)
{
    static uint8_t sr1Val = 0x00;

    testing::InSequence s;
    ExpectSfdpRead(2);
    EXPECT_CALL(spiStub, ReceiveDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x05 }), {}, {}, 0 },
                             hal::QuadSpi::Lines::SingleSpeed()))
        .WillOnce(testing::Return(infra::MakeByteRange(sr1Val)));
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x06 }), {}, {}, 0 }, infra::ConstByteRange{}, hal::QuadSpi::Lines::SingleSpeed()));
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x01 }), {}, {}, 0 }, testing::_, hal::QuadSpi::Lines::SingleSpeed()));
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometryQuadSfdp geometry{ spiStub, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();
}

TEST_F(FlashGeometryQuadSfdpTest, Qer3EnablesQuadViaWriteEnableAndSr2AltCommand)
{
    testing::InSequence s;
    ExpectSfdpRead(3);
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x06 }), {}, {}, 0 }, infra::ConstByteRange{}, hal::QuadSpi::Lines::SingleSpeed()));
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x3E }), {}, {}, 0 }, testing::_, hal::QuadSpi::Lines::SingleSpeed()));
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometryQuadSfdp geometry{ spiStub, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();
}

TEST_F(FlashGeometryQuadSfdpTest, Qer4EnablesQuadViaSr2DirectWrite)
{
    testing::InSequence s;
    ExpectSfdpRead(4);
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x31 }), {}, {}, 0 }, testing::_, hal::QuadSpi::Lines::SingleSpeed()));
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometryQuadSfdp geometry{ spiStub, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();
}

TEST_F(FlashGeometryQuadSfdpTest, Qer5SameSequenceAsQer1)
{
    static uint8_t sr2Val = 0x00;
    static uint8_t sr1Val = 0x00;

    testing::InSequence s;
    ExpectSfdpRead(5);
    EXPECT_CALL(spiStub, ReceiveDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x35 }), {}, {}, 0 },
                             hal::QuadSpi::Lines::SingleSpeed()))
        .WillOnce(testing::Return(infra::MakeByteRange(sr2Val)));
    EXPECT_CALL(spiStub, ReceiveDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x05 }), {}, {}, 0 },
                             hal::QuadSpi::Lines::SingleSpeed()))
        .WillOnce(testing::Return(infra::MakeByteRange(sr1Val)));
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x06 }), {}, {}, 0 }, infra::ConstByteRange{}, hal::QuadSpi::Lines::SingleSpeed()));
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x01 }), {}, {}, 0 }, testing::_, hal::QuadSpi::Lines::SingleSpeed()));
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometryQuadSfdp geometry{ spiStub, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();
}

TEST_F(FlashGeometryQuadSfdpTest, InvalidSfdpSignatureUsesDefaults)
{
    static const std::vector<uint8_t> badHeader(16, 0xFF);

    testing::InSequence s;
    EXPECT_CALL(spiStub, ReceiveDataMock(SfdpHeader(0x000000), hal::QuadSpi::Lines::SingleSpeed()))
        .WillOnce(testing::Return(infra::MakeRange(badHeader.data(), badHeader.data() + badHeader.size())));
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometryQuadSfdp geometry{ spiStub, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();

    EXPECT_EQ(512u, geometry.NrOfSubSectors());
    EXPECT_EQ(4096u, geometry.SizeSubSector());
    EXPECT_EQ(256u, geometry.SizePage());
    EXPECT_FALSE(geometry.ExtendedAddressing());
}
