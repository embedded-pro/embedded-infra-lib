#include "services/tracer/GlobalTracer.hpp"
#include "services/util/TerminalWithStorage.hpp"
#include <gmock/gmock.h>

namespace
{
    class TerminalWithStorageTest
        : public testing::Test
    {
    public:
        services::TerminalWithCommands terminal;
        services::TerminalWithStorage::WithMaxSize<10> terminalCommands{ terminal, services::GlobalTracer() };
    };
}

TEST_F(TerminalWithStorageTest, AddCommand)
{
    terminalCommands.AddCommand({ { "dummy", "d", "Print help menu" }, [](const infra::BoundedConstString&) {} });

    auto commands = terminalCommands.Commands();
    EXPECT_EQ(commands.size(), 2);
    EXPECT_EQ(commands[0].info.longName, "help");
    EXPECT_EQ(commands[1].info.longName, "dummy");
}

TEST_F(TerminalWithStorageTest, InvokeHelp)
{
    auto commands = terminalCommands.Commands();
    commands.begin()->function("");
}
