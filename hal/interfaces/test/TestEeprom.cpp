#include "hal/interfaces/test_doubles/EepromStub.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "gtest/gtest.h"

class EepromTest
    : public testing::Test
    , public infra::EventDispatcherWithWeakPtrFixture
{
public:
    hal::EepromStub eeprom{ 64 };
};

TEST_F(EepromTest, Size_ReturnsConfiguredSize)
{
    EXPECT_EQ(64, eeprom.Size());
}

TEST_F(EepromTest, ReadBuffer_BeforeWrite_ReturnsInitialValue)
{
    std::array<uint8_t, 4> buffer{};
    bool done = false;

    eeprom.ReadBuffer(buffer, 0, [&done]
        {
            done = true;
        });
    ExecuteAllActions();

    EXPECT_TRUE(done);
    EXPECT_EQ(0xff, buffer[0]);
    EXPECT_EQ(0xff, buffer[1]);
    EXPECT_EQ(0xff, buffer[2]);
    EXPECT_EQ(0xff, buffer[3]);
}

TEST_F(EepromTest, WriteBuffer_ThenReadBuffer_ReturnsWrittenData)
{
    std::array<uint8_t, 4> writeData{ 0x01, 0x02, 0x03, 0x04 };
    std::array<uint8_t, 4> readData{};
    bool writeDone = false;
    bool readDone = false;

    eeprom.WriteBuffer(writeData, 0, [&writeDone]
        {
            writeDone = true;
        });
    ExecuteAllActions();

    eeprom.ReadBuffer(readData, 0, [&readDone]
        {
            readDone = true;
        });
    ExecuteAllActions();

    EXPECT_TRUE(writeDone);
    EXPECT_TRUE(readDone);
    EXPECT_EQ(writeData, readData);
}

TEST_F(EepromTest, WriteBuffer_AtOffset_ReturnsWrittenData)
{
    std::array<uint8_t, 3> writeData{ 0xAA, 0xBB, 0xCC };
    std::array<uint8_t, 3> readData{};

    eeprom.WriteBuffer(writeData, 16, []{});
    ExecuteAllActions();

    eeprom.ReadBuffer(readData, 16, []{});
    ExecuteAllActions();

    EXPECT_EQ(writeData, readData);
}

TEST_F(EepromTest, Erase_ResetsAllBytesToInitialValue)
{
    std::array<uint8_t, 4> writeData{ 0x01, 0x02, 0x03, 0x04 };
    std::array<uint8_t, 4> readData{};

    eeprom.WriteBuffer(writeData, 0, []{});
    ExecuteAllActions();

    eeprom.Erase([]{});
    ExecuteAllActions();

    eeprom.ReadBuffer(readData, 0, []{});
    ExecuteAllActions();

    EXPECT_EQ(0xff, readData[0]);
    EXPECT_EQ(0xff, readData[1]);
    EXPECT_EQ(0xff, readData[2]);
    EXPECT_EQ(0xff, readData[3]);
}
