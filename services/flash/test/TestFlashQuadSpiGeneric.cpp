#include "hal/interfaces/test_doubles/QuadSpiStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/flash/FlashQuadSpiGeneric.hpp"
#include "gmock/gmock.h"

namespace
{
    class FlashGeometryQuadStub : public services::FlashGeometryQuad
    {
    public:
        uint32_t NrOfSubSectors() const override
        {
            return 4096;
        }

        uint32_t SizeSector() const override
        {
            return 65536;
        }

        uint32_t SizeSubSector() const override
        {
            return 4096;
        }

        uint32_t SizePage() const override
        {
            return 256;
        }

        bool ExtendedAddressing() const override
        {
            return false;
        }

        uint8_t EraseSubSectorCommand() const override
        {
            return 0x20;
        }

        uint8_t EraseSectorCommand() const override
        {
            return 0xD8;
        }

        uint8_t EraseBulkCommand() const override
        {
            return 0xC7;
        }

        uint8_t PageProgramCommand() const override
        {
            return 0x32;
        }

        uint8_t ReadDataCommand() const override
        {
            return 0xEB;
        }

        uint8_t ReadDummyCycles() const override
        {
            return 10;
        }
    };

    class QuadOutputFlashGeometryQuadStub : public FlashGeometryQuadStub
    {
    public:
        uint8_t ReadDataCommand() const override
        {
            return 0x6B;
        }

        uint8_t ReadDummyCycles() const override
        {
            return 8;
        }

        uint8_t ReadAddressLines() const override
        {
            return 1;
        }
    };

    class LargeFlashGeometryQuadStub : public FlashGeometryQuadStub
    {
    public:
        uint32_t NrOfSubSectors() const override
        {
            return 32768;
        }

        bool ExtendedAddressing() const override
        {
            return true;
        }
    };
}

class FlashQuadSpiGenericTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    testing::StrictMock<hal::QuadSpiStub> spiStub;
    FlashGeometryQuadStub geometry;
    services::FlashQuadSpiGeneric flash{ spiStub, geometry };

    testing::StrictMock<infra::MockCallback<void()>> finished;
};

#define EXPECT_WRITE_ENABLE()                                                                        \
    EXPECT_CALL(spiStub, SendDataMock(                                                               \
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x06 }), {}, {}, 0 }, \
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::QuadSpeed()))

#define EXPECT_POLL_WRITE_DONE()                                                                     \
    EXPECT_CALL(spiStub, PollStatusMock(                                                             \
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x05 }), {}, {}, 0 }, \
                             1, 0, 1, hal::QuadSpi::Lines::QuadSpeed()))

TEST_F(FlashQuadSpiGenericTest, Construction)
{
    EXPECT_EQ(4096u, flash.NumberOfSectors());
    EXPECT_EQ(4096u, flash.SizeOfSector(0));
}

TEST_F(FlashQuadSpiGenericTest, ReadBuffer)
{
    std::array<uint8_t, 4> receiveData = { 0xAA, 0xBB, 0xCC, 0xDD };
    EXPECT_CALL(spiStub, ReceiveDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0xEB }), hal::QuadSpi::AddressToVector(0x1000, 3), {}, 10 },
                             hal::QuadSpi::Lines::QuadSpeed()))
        .WillOnce(testing::Return(infra::MakeByteRange(receiveData)));
    EXPECT_CALL(finished, callback());

    std::array<uint8_t, 4> buffer{};
    flash.ReadBuffer(buffer, 0x1000, [this]()
        {
            finished.callback();
        });
    ExecuteAllActions();

    EXPECT_EQ(receiveData, buffer);
}

TEST_F(FlashQuadSpiGenericTest, WriteBuffer)
{
    const std::array<uint8_t, 4> sendData = { 1, 2, 3, 4 };
    EXPECT_WRITE_ENABLE();
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x32 }), hal::QuadSpi::AddressToVector(0, 3), {}, 0 },
                             infra::MakeByteRange(sendData), hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE();

    flash.WriteBuffer(sendData, 0, [this]()
        {
            finished.callback();
        });
    ExecuteAllActions();

    EXPECT_CALL(finished, callback());
    spiStub.onDone();
    ExecuteAllActions();
}

TEST_F(FlashQuadSpiGenericTest, WriteBufferAtNonZeroAddress)
{
    const std::array<uint8_t, 4> sendData = { 1, 2, 3, 4 };
    EXPECT_WRITE_ENABLE();
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x32 }), hal::QuadSpi::AddressToVector(0x5000, 3), {}, 0 },
                             infra::MakeByteRange(sendData), hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE();

    flash.WriteBuffer(sendData, 0x5000, infra::emptyFunction);
    ExecuteAllActions();

    spiStub.onDone();
    ExecuteAllActions();
}

TEST_F(FlashQuadSpiGenericTest, EraseSubSector)
{
    EXPECT_WRITE_ENABLE();
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x20 }), hal::QuadSpi::AddressToVector(0, 3), {}, 0 },
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE();

    flash.EraseSector(0, [this]()
        {
            finished.callback();
        });
    ExecuteAllActions();

    EXPECT_CALL(finished, callback());
    spiStub.onDone();
    ExecuteAllActions();
}

TEST_F(FlashQuadSpiGenericTest, EraseSector)
{
    EXPECT_WRITE_ENABLE();
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0xD8 }), hal::QuadSpi::AddressToVector(0, 3), {}, 0 },
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE();

    flash.EraseSectors(0, 16, [this]()
        {
            finished.callback();
        });
    ExecuteAllActions();

    EXPECT_CALL(finished, callback());
    spiStub.onDone();
    ExecuteAllActions();
}

TEST_F(FlashQuadSpiGenericTest, EraseAll)
{
    EXPECT_WRITE_ENABLE();
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0xC7 }), {}, {}, 0 },
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE();

    flash.EraseAll([this]()
        {
            finished.callback();
        });
    ExecuteAllActions();

    EXPECT_CALL(finished, callback());
    spiStub.onDone();
    ExecuteAllActions();
}

TEST_F(FlashQuadSpiGenericTest, WriteBufferSplitsAcrossPageBoundary)
{
    // Write 6 bytes starting at offset 254 in the first page.
    // First page program: 2 bytes (254..255), second page program: 4 bytes (256..259).
    const std::array<uint8_t, 6> sendData = { 1, 2, 3, 4, 5, 6 };

    testing::InSequence s;
    EXPECT_WRITE_ENABLE();
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x32 }), hal::QuadSpi::AddressToVector(254, 3), {}, 0 },
                             infra::MakeRange(sendData.data(), sendData.data() + 2), hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE();
    EXPECT_WRITE_ENABLE();
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x32 }), hal::QuadSpi::AddressToVector(256, 3), {}, 0 },
                             infra::MakeRange(sendData.data() + 2, sendData.data() + 6), hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE();

    flash.WriteBuffer(sendData, 254, infra::emptyFunction);
    ExecuteAllActions();
    spiStub.onDone();
    ExecuteAllActions();
    spiStub.onDone();
    ExecuteAllActions();
}

TEST_F(FlashQuadSpiGenericTest, EraseMixedSubSectorAndSector)
{
    // Erase sub-sector at index 15, then full sector at index 16, then sub-sector at index 32.
    testing::InSequence s;

    EXPECT_WRITE_ENABLE();
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x20 }), hal::QuadSpi::AddressToVector(15 * 4096, 3), {}, 0 },
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE();
    EXPECT_WRITE_ENABLE();
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0xD8 }), hal::QuadSpi::AddressToVector(16 * 4096, 3), {}, 0 },
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE();
    EXPECT_WRITE_ENABLE();
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x20 }), hal::QuadSpi::AddressToVector(32 * 4096, 3), {}, 0 },
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE();

    flash.EraseSectors(15, 33, infra::emptyFunction);
    ExecuteAllActions();
    spiStub.onDone();
    ExecuteAllActions();
    spiStub.onDone();
    ExecuteAllActions();
    spiStub.onDone();
    ExecuteAllActions();
}

#define EXPECT_WRITE_ENABLE_ON(lines)                                                                \
    EXPECT_CALL(spiStub, SendDataMock(                                                               \
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x06 }), {}, {}, 0 }, \
                             infra::ConstByteRange{}, lines))

#define EXPECT_POLL_WRITE_DONE_ON(lines)                                                             \
    EXPECT_CALL(spiStub, PollStatusMock(                                                             \
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x05 }), {}, {}, 0 }, \
                             1, 0, 1, lines))

class FlashQuadSpiGenericExtendedAddressingTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    testing::StrictMock<hal::QuadSpiStub> spiStub;
    LargeFlashGeometryQuadStub geometry;
    services::FlashQuadSpiGeneric flash{ spiStub, geometry };

    testing::StrictMock<infra::MockCallback<void()>> finished;
};

TEST_F(FlashQuadSpiGenericExtendedAddressingTest, ReadBufferUsesTheFourByteCommandAndAddress)
{
    std::array<uint8_t, 4> receiveData = { 0xAA, 0xBB, 0xCC, 0xDD };
    EXPECT_CALL(spiStub, ReceiveDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0xEC }), hal::QuadSpi::AddressToVector(0x3FFF000, 4), {}, 10 },
                             hal::QuadSpi::Lines::QuadSpeed()))
        .WillOnce(testing::Return(infra::MakeByteRange(receiveData)));
    EXPECT_CALL(finished, callback());

    std::array<uint8_t, 4> buffer{};
    flash.ReadBuffer(buffer, 0x3FFF000, [this]()
        {
            finished.callback();
        });
    ExecuteAllActions();

    EXPECT_EQ(receiveData, buffer);
}

TEST_F(FlashQuadSpiGenericExtendedAddressingTest, WriteBufferUsesTheFourByteCommandAndAddress)
{
    const std::array<uint8_t, 4> sendData = { 1, 2, 3, 4 };
    EXPECT_WRITE_ENABLE_ON(hal::QuadSpi::Lines::QuadSpeed());
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x34 }), hal::QuadSpi::AddressToVector(0x3FFF000, 4), {}, 0 },
                             infra::MakeByteRange(sendData), hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE_ON(hal::QuadSpi::Lines::QuadSpeed());

    flash.WriteBuffer(sendData, 0x3FFF000, infra::emptyFunction);
    ExecuteAllActions();
    spiStub.onDone();
    ExecuteAllActions();
}

TEST_F(FlashQuadSpiGenericExtendedAddressingTest, EraseSubSectorUsesTheFourByteCommandAndAddress)
{
    EXPECT_WRITE_ENABLE_ON(hal::QuadSpi::Lines::QuadSpeed());
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x21 }), hal::QuadSpi::AddressToVector(32767 * 4096, 4), {}, 0 },
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE_ON(hal::QuadSpi::Lines::QuadSpeed());

    flash.EraseSector(32767, infra::emptyFunction);
    ExecuteAllActions();
    spiStub.onDone();
    ExecuteAllActions();
}

TEST_F(FlashQuadSpiGenericExtendedAddressingTest, EraseSectorUsesTheFourByteCommandAndAddress)
{
    EXPECT_WRITE_ENABLE_ON(hal::QuadSpi::Lines::QuadSpeed());
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0xDC }), hal::QuadSpi::AddressToVector(16 * 4096, 4), {}, 0 },
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE_ON(hal::QuadSpi::Lines::QuadSpeed());

    flash.EraseSectors(16, 32, infra::emptyFunction);
    ExecuteAllActions();
    spiStub.onDone();
    ExecuteAllActions();
}

TEST_F(FlashQuadSpiGenericExtendedAddressingTest, EraseAllKeepsTheBulkEraseCommand)
{
    EXPECT_WRITE_ENABLE_ON(hal::QuadSpi::Lines::QuadSpeed());
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0xC7 }), {}, {}, 0 },
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::QuadSpeed()));
    EXPECT_POLL_WRITE_DONE_ON(hal::QuadSpi::Lines::QuadSpeed());

    flash.EraseAll(infra::emptyFunction);
    ExecuteAllActions();
    spiStub.onDone();
    ExecuteAllActions();
}

class FlashQuadSpiGenericExtendedSpiTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    testing::StrictMock<hal::QuadSpiStub> spiStub;
    FlashGeometryQuadStub geometry;
    services::FlashQuadSpiGeneric flash{ spiStub, geometry, services::FlashQuadSpiGeneric::Protocol::extendedSpi };

    testing::StrictMock<infra::MockCallback<void()>> finished;
};

TEST_F(FlashQuadSpiGenericExtendedSpiTest, ReadBufferSendsTheCommandOnOneLineAndTheAddressAndDataOnFour)
{
    std::array<uint8_t, 4> receiveData = { 0xAA, 0xBB, 0xCC, 0xDD };
    EXPECT_CALL(spiStub, ReceiveDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0xEB }), hal::QuadSpi::AddressToVector(0x1000, 3), {}, 10 },
                             hal::QuadSpi::Lines::MixedSpeed(1, 4, 4)))
        .WillOnce(testing::Return(infra::MakeByteRange(receiveData)));
    EXPECT_CALL(finished, callback());

    std::array<uint8_t, 4> buffer{};
    flash.ReadBuffer(buffer, 0x1000, [this]()
        {
            finished.callback();
        });
    ExecuteAllActions();

    EXPECT_EQ(receiveData, buffer);
}

TEST_F(FlashQuadSpiGenericExtendedSpiTest, WriteBufferSendsTheCommandAndTheAddressOnOneLineAndTheDataOnFour)
{
    const std::array<uint8_t, 4> sendData = { 1, 2, 3, 4 };
    EXPECT_WRITE_ENABLE_ON(hal::QuadSpi::Lines::SingleSpeed());
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x32 }), hal::QuadSpi::AddressToVector(0x5000, 3), {}, 0 },
                             infra::MakeByteRange(sendData), hal::QuadSpi::Lines::MixedSpeed(1, 1, 4)));
    EXPECT_POLL_WRITE_DONE_ON(hal::QuadSpi::Lines::SingleSpeed());

    flash.WriteBuffer(sendData, 0x5000, [this]()
        {
            finished.callback();
        });
    ExecuteAllActions();

    EXPECT_CALL(finished, callback());
    spiStub.onDone();
    ExecuteAllActions();
}

TEST_F(FlashQuadSpiGenericExtendedSpiTest, EraseSubSectorSendsEverythingOnOneLine)
{
    EXPECT_WRITE_ENABLE_ON(hal::QuadSpi::Lines::SingleSpeed());
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x20 }), hal::QuadSpi::AddressToVector(0, 3), {}, 0 },
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::SingleSpeed()));
    EXPECT_POLL_WRITE_DONE_ON(hal::QuadSpi::Lines::SingleSpeed());

    flash.EraseSector(0, infra::emptyFunction);
    ExecuteAllActions();
    spiStub.onDone();
    ExecuteAllActions();
}

TEST_F(FlashQuadSpiGenericExtendedSpiTest, EraseAllSendsEverythingOnOneLine)
{
    EXPECT_WRITE_ENABLE_ON(hal::QuadSpi::Lines::SingleSpeed());
    EXPECT_CALL(spiStub, SendDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0xC7 }), {}, {}, 0 },
                             infra::ConstByteRange{}, hal::QuadSpi::Lines::SingleSpeed()));
    EXPECT_POLL_WRITE_DONE_ON(hal::QuadSpi::Lines::SingleSpeed());

    flash.EraseAll(infra::emptyFunction);
    ExecuteAllActions();
    spiStub.onDone();
    ExecuteAllActions();
}

class FlashQuadSpiGenericExtendedSpiExtendedAddressingTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    testing::StrictMock<hal::QuadSpiStub> spiStub;
    LargeFlashGeometryQuadStub geometry;
    services::FlashQuadSpiGeneric flash{ spiStub, geometry, services::FlashQuadSpiGeneric::Protocol::extendedSpi };
};

TEST_F(FlashQuadSpiGenericExtendedSpiExtendedAddressingTest, ReadBufferUsesTheFourByteCommandAndAddressOnTheExtendedSpiLines)
{
    std::array<uint8_t, 2> receiveData = { 0x12, 0x34 };
    EXPECT_CALL(spiStub, ReceiveDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0xEC }), hal::QuadSpi::AddressToVector(0x3FFF000, 4), {}, 10 },
                             hal::QuadSpi::Lines::MixedSpeed(1, 4, 4)))
        .WillOnce(testing::Return(infra::MakeByteRange(receiveData)));

    std::array<uint8_t, 2> buffer{};
    flash.ReadBuffer(buffer, 0x3FFF000, infra::emptyFunction);
    ExecuteAllActions();

    EXPECT_EQ(receiveData, buffer);
}

TEST_F(FlashQuadSpiGenericExtendedSpiTest, ReadBufferSendsTheAddressOnOneLineWhenTheFlashHasOnlyAQuadOutputRead)
{
    QuadOutputFlashGeometryQuadStub quadOutputGeometry;
    services::FlashQuadSpiGeneric quadOutputFlash{ spiStub, quadOutputGeometry, services::FlashQuadSpiGeneric::Protocol::extendedSpi };

    std::array<uint8_t, 2> receiveData = { 0x12, 0x34 };
    EXPECT_CALL(spiStub, ReceiveDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x6B }), hal::QuadSpi::AddressToVector(0x1000, 3), {}, 8 },
                             hal::QuadSpi::Lines::MixedSpeed(1, 1, 4)))
        .WillOnce(testing::Return(infra::MakeByteRange(receiveData)));

    std::array<uint8_t, 2> buffer{};
    quadOutputFlash.ReadBuffer(buffer, 0x1000, infra::emptyFunction);
    ExecuteAllActions();

    EXPECT_EQ(receiveData, buffer);
}

TEST_F(FlashQuadSpiGenericTest, ReadBufferInQuadModeUsesFourLinesForAQuadOutputRead)
{
    QuadOutputFlashGeometryQuadStub quadOutputGeometry;
    services::FlashQuadSpiGeneric quadOutputFlash{ spiStub, quadOutputGeometry };

    std::array<uint8_t, 2> receiveData = { 0x12, 0x34 };
    EXPECT_CALL(spiStub, ReceiveDataMock(
                             hal::QuadSpi::Header{ std::make_optional(uint8_t{ 0x6B }), hal::QuadSpi::AddressToVector(0x1000, 3), {}, 8 },
                             hal::QuadSpi::Lines::QuadSpeed()))
        .WillOnce(testing::Return(infra::MakeByteRange(receiveData)));

    std::array<uint8_t, 2> buffer{};
    quadOutputFlash.ReadBuffer(buffer, 0x1000, infra::emptyFunction);
    ExecuteAllActions();

    EXPECT_EQ(receiveData, buffer);
}
