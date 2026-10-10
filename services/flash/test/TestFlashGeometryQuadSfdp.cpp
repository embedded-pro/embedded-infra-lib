#include "hal/interfaces/test_doubles/QuadSpiStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/flash/FlashGeometryQuadSfdp.hpp"
#include "gmock/gmock.h"
#include <optional>

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
        bfpt[28] = 0x0C;
        bfpt[29] = 0x20;
        bfpt[30] = 0x10;
        bfpt[31] = 0xD8;
        bfpt[40] = 0x80;                                    // DW11: 256-byte page
        bfpt[58] = static_cast<uint8_t>((qer & 0x07) << 4); // DW15: QER
        return bfpt;
    }

    constexpr uint32_t fastRead144Supported = 1u << 21;
    constexpr uint32_t fastRead114Supported = 1u << 22;

    constexpr uint32_t FastReadField(uint8_t instruction, uint8_t modeClocks, uint8_t waitStates)
    {
        return static_cast<uint32_t>(instruction) << 8 | static_cast<uint32_t>(modeClocks) << 5 | waitStates;
    }

    void SetDword(std::vector<uint8_t>& bfpt, std::size_t dword, uint32_t value)
    {
        for (std::size_t byte = 0; byte != 4; ++byte)
            bfpt[(dword - 1) * 4 + byte] = static_cast<uint8_t>(value >> (8 * byte));
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

TEST_F(FlashGeometryQuadSfdpTest, UnknownQerValueFiresOnInitializedWithoutExtraSpi)
{
    // QER = 7 hits the default case → no extra SPI calls
    testing::InSequence s;
    ExpectSfdpRead(7);
    EXPECT_CALL(onInitialized, callback());

    services::FlashGeometryQuadSfdp geometry{ spiStub, [this]()
        {
            onInitialized.callback();
        } };
    ExecuteAllActions();
}

class FlashGeometryQuadSfdpFastReadTest
    : public FlashGeometryQuadSfdpTest
{
public:
    void Initialize(std::vector<uint8_t> bfpt)
    {
        sfdp = bfpt;

        testing::InSequence s;
        EXPECT_CALL(spiStub, ReceiveDataMock(SfdpHeader(0x000000), hal::QuadSpi::Lines::SingleSpeed()))
            .WillOnce(testing::Return(infra::MakeRange(sfdpHeader.data(), sfdpHeader.data() + sfdpHeader.size())));
        EXPECT_CALL(spiStub, ReceiveDataMock(SfdpHeader(0x000080), hal::QuadSpi::Lines::SingleSpeed()))
            .WillOnce(testing::Return(infra::MakeRange(sfdp.data(), sfdp.data() + sfdp.size())));
        EXPECT_CALL(onInitialized, callback());

        geometry.emplace(spiStub, [this]()
            {
                onInitialized.callback();
            });
        ExecuteAllActions();
    }

    std::vector<uint8_t> sfdp;
    std::optional<services::FlashGeometryQuadSfdp> geometry;
};

TEST_F(FlashGeometryQuadSfdpFastReadTest, TheFastReadOf144IsParsedFromTheLowerHalfOfDword3)
{
    auto bfpt = MakeBfptWithQer(0);
    SetDword(bfpt, 1, fastRead144Supported);
    SetDword(bfpt, 3, FastReadField(0xEB, 2, 8) | FastReadField(0x6B, 1, 7) << 16);
    Initialize(bfpt);

    EXPECT_EQ(0xEB, geometry->ReadDataCommand());
    EXPECT_EQ(10u, geometry->ReadDummyCycles());
    EXPECT_EQ(4u, geometry->ReadAddressLines());
}

TEST_F(FlashGeometryQuadSfdpFastReadTest, TheFastReadOf114IsUsedWhenThe144IsNotSupported)
{
    auto bfpt = MakeBfptWithQer(0);
    SetDword(bfpt, 1, fastRead114Supported);
    SetDword(bfpt, 3, FastReadField(0x6B, 1, 7) << 16);
    Initialize(bfpt);

    EXPECT_EQ(0x6B, geometry->ReadDataCommand());
    EXPECT_EQ(8u, geometry->ReadDummyCycles());
    EXPECT_EQ(1u, geometry->ReadAddressLines());
}

TEST_F(FlashGeometryQuadSfdpFastReadTest, TheFastReadOf144IsPreferredWhenBothAreSupported)
{
    auto bfpt = MakeBfptWithQer(0);
    SetDword(bfpt, 1, fastRead144Supported | fastRead114Supported);
    SetDword(bfpt, 3, FastReadField(0xEB, 1, 9) | FastReadField(0x6B, 1, 7) << 16);
    Initialize(bfpt);

    EXPECT_EQ(0xEB, geometry->ReadDataCommand());
    EXPECT_EQ(10u, geometry->ReadDummyCycles());
    EXPECT_EQ(4u, geometry->ReadAddressLines());
}

TEST_F(FlashGeometryQuadSfdpFastReadTest, WithoutAQuadFastReadTheDefaultsAreKept)
{
    auto bfpt = MakeBfptWithQer(0);
    SetDword(bfpt, 3, FastReadField(0x3B, 0, 1) | FastReadField(0x6B, 0, 7) << 16);
    Initialize(bfpt);

    EXPECT_EQ(0xEB, geometry->ReadDataCommand());
    EXPECT_EQ(10u, geometry->ReadDummyCycles());
    EXPECT_EQ(4u, geometry->ReadAddressLines());
}

TEST_F(FlashGeometryQuadSfdpFastReadTest, TheSfdpOfAnMt25ql512IsParsed)
{
    static const std::vector<uint8_t> header{ 0x53, 0x46, 0x44, 0x50, 0x06, 0x01, 0x01, 0xff, 0x00, 0x06, 0x01, 0x10, 0x30, 0x00, 0x00, 0xff };
    static const std::vector<uint8_t> bfpt{
        0xe5, 0x20, 0xfb, 0xff, 0xff, 0xff, 0xff, 0x1f, 0x29, 0xeb, 0x27, 0x6b, 0x27, 0x3b, 0x27, 0xbb,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x27, 0xbb, 0xff, 0xff, 0x29, 0xeb, 0x0c, 0x20, 0x10, 0xd8,
        0x0f, 0x52, 0x00, 0x00, 0x24, 0x4a, 0x99, 0x00, 0x8b, 0x8e, 0x03, 0xe1, 0xac, 0x01, 0x27, 0x38,
        0x7a, 0x75, 0x7a, 0x75, 0xfb, 0xbd, 0xd5, 0x5c, 0x4a, 0x0f, 0x82, 0xff, 0x81, 0xbd, 0x3d, 0x36
    };

    testing::InSequence s;
    EXPECT_CALL(spiStub, ReceiveDataMock(SfdpHeader(0x000000), hal::QuadSpi::Lines::SingleSpeed()))
        .WillOnce(testing::Return(infra::MakeRange(header.data(), header.data() + header.size())));
    EXPECT_CALL(spiStub, ReceiveDataMock(SfdpHeader(0x000030), hal::QuadSpi::Lines::SingleSpeed()))
        .WillOnce(testing::Return(infra::MakeRange(bfpt.data(), bfpt.data() + bfpt.size())));
    EXPECT_CALL(onInitialized, callback());

    geometry.emplace(spiStub, [this]()
        {
            onInitialized.callback();
        });
    ExecuteAllActions();

    EXPECT_EQ(16384u, geometry->NrOfSubSectors());
    EXPECT_EQ(4096u, geometry->SizeSubSector());
    EXPECT_EQ(65536u, geometry->SizeSector());
    EXPECT_EQ(256u, geometry->SizePage());
    EXPECT_TRUE(geometry->ExtendedAddressing());
    EXPECT_EQ(0x20, geometry->EraseSubSectorCommand());
    EXPECT_EQ(0xD8, geometry->EraseSectorCommand());
    EXPECT_EQ(0xEB, geometry->ReadDataCommand());
    EXPECT_EQ(10u, geometry->ReadDummyCycles());
    EXPECT_EQ(4u, geometry->ReadAddressLines());
}
