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
