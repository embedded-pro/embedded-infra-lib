#include "hal/synchronous_interfaces/test_doubles/SynchronousQuadSpiMock.hpp"
#include "services/synchronous_flash/SynchronousFlashQuadSpi.hpp"
#include "gmock/gmock.h"
#include <limits>

class SynchronousFlashQuadSpiTest : public testing::Test
{
public:
    SynchronousFlashQuadSpiTest()
    {
        ExpectConstructorCalls();
    }

    void ExpectConstructorCalls()
    {
        testing::InSequence s;

        EXPECT_CALL(quadSpi, SendDataMock(testing::_, testing::IsEmpty()));
        EXPECT_CALL(quadSpi, SendDataMock(testing::_, testing::ElementsAre(0x5f)));
    }

    void ExpectWriteEnable()
    {
        EXPECT_CALL(quadSpi, SendDataQuadMock(testing::_, testing::IsEmpty()));
    }

    void ExpectWriteDone()
    {
        EXPECT_CALL(quadSpi, ReceiveDataQuadMock(testing::_))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
    }

    void ExpectWriteInProgressThenDone()
    {
        EXPECT_CALL(quadSpi, ReceiveDataQuadMock(testing::_))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x01 }))
            .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
    }

    hal::SynchronousQuadSpiMock quadSpi;
};

TEST_F(SynchronousFlashQuadSpiTest, Construction)
{
    services::SynchronousFlashQuadSpi flash(quadSpi);
}

TEST_F(SynchronousFlashQuadSpiTest, ReadBuffer)
{
    services::SynchronousFlashQuadSpi flash(quadSpi);

    std::array<uint8_t, 4> buffer{};
    EXPECT_CALL(quadSpi, ReceiveDataQuadMock(testing::_))
        .WillOnce(testing::Return(std::vector<uint8_t>{ 0xAA, 0xBB, 0xCC, 0xDD }));

    flash.ReadBuffer(buffer, 0x1000);

    EXPECT_EQ(buffer[0], 0xAA);
    EXPECT_EQ(buffer[1], 0xBB);
    EXPECT_EQ(buffer[2], 0xCC);
    EXPECT_EQ(buffer[3], 0xDD);
}

TEST_F(SynchronousFlashQuadSpiTest, WriteBufferSinglePage)
{
    services::SynchronousFlashQuadSpi flash(quadSpi);

    std::array<uint8_t, 4> buffer{ 0x01, 0x02, 0x03, 0x04 };

    {
        testing::InSequence s;
        ExpectWriteEnable();
        EXPECT_CALL(quadSpi, SendDataQuadMock(testing::_, testing::ElementsAre(0x01, 0x02, 0x03, 0x04)));
        ExpectWriteDone();
    }

    flash.WriteBuffer(buffer, 0);
}

TEST_F(SynchronousFlashQuadSpiTest, WriteBufferSpanningPageBoundary)
{
    services::SynchronousFlashQuadSpi flash(quadSpi);

    std::array<uint8_t, 4> buffer{ 0x01, 0x02, 0x03, 0x04 };

    {
        testing::InSequence s;

        ExpectWriteEnable();
        EXPECT_CALL(quadSpi, SendDataQuadMock(testing::_, testing::SizeIs(2)));
        ExpectWriteDone();

        ExpectWriteEnable();
        EXPECT_CALL(quadSpi, SendDataQuadMock(testing::_, testing::SizeIs(2)));
        ExpectWriteDone();
    }

    flash.WriteBuffer(buffer, 254);
}

TEST_F(SynchronousFlashQuadSpiTest, WriteBufferWaitsForWriteCompletion)
{
    services::SynchronousFlashQuadSpi flash(quadSpi);

    std::array<uint8_t, 1> buffer{ 0xFF };

    {
        testing::InSequence s;
        ExpectWriteEnable();
        EXPECT_CALL(quadSpi, SendDataQuadMock(testing::_, testing::ElementsAre(0xFF)));
        ExpectWriteInProgressThenDone();
    }

    flash.WriteBuffer(buffer, 0);
}

TEST_F(SynchronousFlashQuadSpiTest, EraseSectorsIsNoOp)
{
    services::SynchronousFlashQuadSpi flash(quadSpi);

    flash.EraseSectors(0, 1);
}

TEST_F(SynchronousFlashQuadSpiTest, NumberOfSectors)
{
    services::SynchronousFlashQuadSpi flash(quadSpi);

    EXPECT_EQ(flash.NumberOfSectors(), 4096u);
}

TEST_F(SynchronousFlashQuadSpiTest, SizeOfSector)
{
    services::SynchronousFlashQuadSpi flash(quadSpi);

    EXPECT_EQ(flash.SizeOfSector(0), 4096u);
}
