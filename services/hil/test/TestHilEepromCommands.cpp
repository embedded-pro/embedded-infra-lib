#include "hal/interfaces/test_doubles/EepromMock.hpp"
#include "infra/util/test_helper/MockHelpers.hpp"
#include "services/hil/commands/HilEepromCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

namespace
{
    class EepromFactoryStub
        : public services::HilEepromFactory
    {
    public:
        hal::Eeprom& Instance() override
        {
            ++instances;
            return eeprom;
        }

        uint32_t instances = 0;
        testing::StrictMock<hal::CleanEepromMock> eeprom;
    };
}

class HilEepromCommandsTest
    : public services::HilFixture
{
public:
    HilEepromCommandsTest()
    {
        EXPECT_CALL(factory.eeprom, Size()).WillRepeatedly(testing::Return(16));
    }

    EepromFactoryStub factory;
    services::HilEepromCommands::WithCapacity<4> eeprom{ context, factory };
    infra::Function<void()> onDone;
};

TEST_F(HilEepromCommandsTest, eeprom_is_obtained_on_first_use)
{
    EXPECT_EQ(0, factory.instances);
}

TEST_F(HilEepromCommandsTest, write_reports_from_completion)
{
    EXPECT_CALL(factory.eeprom, WriteBuffer(infra::CheckByteRangeContents(std::vector<uint8_t>{ 0x01, 0x02 }), 4, testing::_)).WillOnce(testing::SaveArg<2>(&onDone));
    Execute("eeprom.write 4 0102");
    Execute("eeprom.erase");
    EXPECT_EQ("ERR busy\r\n", Output());

    onDone();
    EXPECT_EQ("", Output());
    ExecuteAllActions();
    EXPECT_EQ("\r\nOK\r\n", Output());
}

TEST_F(HilEepromCommandsTest, read_reports_data_from_completion)
{
    EXPECT_CALL(factory.eeprom, ReadBuffer(testing::_, 2, testing::_)).WillOnce(testing::Invoke([](infra::ByteRange buffer, uint32_t, infra::Function<void()> onDone)
        {
            buffer[0] = 0xca;
            buffer[1] = 0xfe;
            onDone();
        }));
    Execute("eeprom.read 2 2");
    EXPECT_EQ("", Output());

    ExecuteAllActions();
    EXPECT_EQ("\r\nOK data=cafe\r\n", Output());
}

TEST_F(HilEepromCommandsTest, erase_reports_from_completion)
{
    EXPECT_CALL(factory.eeprom, Erase(testing::_)).WillOnce(testing::SaveArg<0>(&onDone));
    Execute("eeprom.erase");
    onDone();
    ExecuteAllActions();

    EXPECT_EQ("\r\nOK\r\n", Output());
}

TEST_F(HilEepromCommandsTest, operation_times_out)
{
    EXPECT_CALL(factory.eeprom, Erase(testing::_)).WillOnce(testing::SaveArg<0>(&onDone));
    Execute("eeprom.erase");
    ForwardTime(std::chrono::seconds(5));
    EXPECT_EQ("\r\nERR timeout\r\n", Output());

    onDone();
    ExecuteAllActions();
    EXPECT_EQ("", Output());
}

TEST_F(HilEepromCommandsTest, arguments_are_checked)
{
    Execute("eeprom.write 17 00");
    Execute("eeprom.write 15 0102");
    Execute("eeprom.write 0 -");
    Execute("eeprom.write 0 0011223344");
    Execute("eeprom.read 0 5");
    Execute("eeprom.read 14 3");
    Execute("eeprom.erase 1");

    EXPECT_EQ("ERR range\r\nERR range\r\nERR usage\r\nERR range\r\nERR range\r\nERR range\r\nERR usage\r\n", Output());
}
