#include "hal/interfaces/test_doubles/ResetMock.hpp"
#include "services/hil/SystemCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"
#include <array>

namespace
{
    class BoardInfoStub
        : public services::hil::BoardInfo
    {
    public:
        const char* Name() const override
        {
            return "EK-TM4C123GXL";
        }

        const char* Family() const override
        {
            return "tm4c123";
        }

        uint32_t SystemClock() const override
        {
            return 80000000;
        }

        const char* ResetCause() const override
        {
            return "por";
        }

        infra::ConstByteRange UniqueId() const override
        {
            return uid;
        }

        infra::ConstByteRange uid;
    };
}

class SystemCommandsTest
    : public services::hil::HilFixture
{
public:
    BoardInfoStub board;
    testing::StrictMock<hal::ResetMock> reset;
    services::hil::SystemCommands system{ context, board, reset };
};

TEST_F(SystemCommandsTest, ping_answers_ok)
{
    Execute("ping");
    Execute("ping 1");

    EXPECT_EQ("OK\r\nERR usage\r\n", Output());
}

TEST_F(SystemCommandsTest, boot_event_is_printed_on_a_new_line)
{
    system.PrintBoot();

    EXPECT_EQ("\r\nEVT boot board=EK-TM4C123GXL family=tm4c123 sysclk=80000000 reset=por\r\n", Output());
}

TEST_F(SystemCommandsTest, info_without_unique_id)
{
    Execute("info");

    EXPECT_EQ("OK board=EK-TM4C123GXL family=tm4c123 sysclk=80000000 reset=por uid=none\r\n", Output());
}

TEST_F(SystemCommandsTest, info_with_unique_id)
{
    const std::array<uint8_t, 3> uid{ { 0x01, 0xab, 0x10 } };
    board.uid = infra::MakeRange(uid);

    Execute("info");

    EXPECT_EQ("OK board=EK-TM4C123GXL family=tm4c123 sysclk=80000000 reset=por uid=01ab10\r\n", Output());
}

TEST_F(SystemCommandsTest, delay_answers_after_the_delay)
{
    Execute("delay 100");
    EXPECT_EQ("", Output());

    ForwardTime(std::chrono::milliseconds(99));
    EXPECT_EQ("", Output());

    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_EQ("\r\nOK\r\n", Output());
}

TEST_F(SystemCommandsTest, delay_is_busy_while_armed)
{
    Execute("delay 100");
    Execute("delay 1");

    EXPECT_EQ("ERR busy\r\n", Output());
}

TEST_F(SystemCommandsTest, delay_checks_its_argument)
{
    Execute("delay 600001");
    Execute("delay x");
    Execute("delay");

    EXPECT_EQ("ERR range\r\nERR usage\r\nERR usage\r\n", Output());
}

TEST_F(SystemCommandsTest, reset_resets_after_flush_delay)
{
    Execute("reset");
    ForwardTime(std::chrono::milliseconds(19));

    EXPECT_CALL(reset, ResetModule(testing::_));
    ForwardTime(std::chrono::milliseconds(1));

    EXPECT_EQ("", Output());
}

TEST_F(SystemCommandsTest, board_pins_lists_aliases)
{
    Execute("board.pins");

    EXPECT_EQ("OK led=PF1,id0=PC3,vbus=PE0\r\n", Output());
}
