#include "services/hil/commands/HilUnsupportedCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

namespace
{
    constexpr std::array<const char*, 3> names{ { "eth.open", "eth.status", "eth.close" } };
}

class HilUnsupportedCommandsTest
    : public services::HilFixture
{
public:
    services::HilUnsupportedCommands::WithMaxCommands<3> unsupported{ context, infra::MakeRange(names) };
};

TEST_F(HilUnsupportedCommandsTest, listed_commands_answer_unsupported)
{
    Execute("eth.open speed=100");
    Execute("eth.status");
    Execute("eth.close");

    EXPECT_EQ("ERR unsupported\r\nERR unsupported\r\nERR unsupported\r\n", Output());
}
