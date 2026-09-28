#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "services/hil/Command.hpp"
#include "services/hil/HilTerminal.hpp"
#include "gmock/gmock.h"
#include <array>
#include <string>
#include <vector>

namespace
{
    class ExampleCommands
        : public services::TerminalCommands
    {
    public:
        ExampleCommands(services::TerminalWithCommands& terminal, services::hil::Response& response)
            : services::TerminalCommands(terminal)
            , response(response)
            , commands{ {
                  services::hil::Bind<ExampleCommands, &ExampleCommands::Ping>("ping", "", *this, response),
                  services::hil::Bind<ExampleCommands, &ExampleCommands::Fail>("fail", "", *this, response),
              } }
        {}

        infra::MemoryRange<const Command> Commands() override
        {
            return infra::MakeRange(commands);
        }

    private:
        services::hil::Status Ping(const services::hil::Arguments& arguments)
        {
            if (!arguments.Shape(0, 0, {}))
                return services::hil::Status::usage;

            response.Ok();
            return services::hil::Status::done;
        }

        services::hil::Status Fail(const services::hil::Arguments&)
        {
            return services::hil::Status::range;
        }

    private:
        services::hil::Response& response;
        std::array<Command, 2> commands;
    };
}

class HilTerminalTest
    : public testing::Test
    , public infra::EventDispatcherWithWeakPtrFixture
{
public:
    std::string Output()
    {
        std::string result(stream.Storage().begin(), stream.Storage().end());
        stream.Storage().clear();
        return result;
    }

    void Receive(const std::string& text)
    {
        communication.dataReceived(std::vector<uint8_t>(text.begin(), text.end()));
        ExecuteAllActions();
    }

    infra::StringOutputStream::WithStorage<256> stream;
    services::TracerToStream tracer{ stream };
    services::hil::Response response{ tracer };
    testing::StrictMock<hal::SerialCommunicationMock> communication;
    services::hil::HilTerminal::WithMaxQueueAndMaxHistory<32, 1> terminal{ communication, tracer, response };
    ExampleCommands commands{ terminal, response };
};

TEST_F(HilTerminalTest, prints_prompt_on_construction)
{
    EXPECT_EQ("> ", Output());
}

TEST_F(HilTerminalTest, command_output_follows_echo_without_extra_prefix)
{
    Output();

    Receive("ping\r");

    EXPECT_EQ("ping\r\nOK\r\n> ", Output());
}

TEST_F(HilTerminalTest, command_error_is_reported)
{
    Output();

    Receive("fail\r");
    Receive("ping 1\r");

    EXPECT_EQ("fail\r\nERR range\r\n> ping 1\r\nERR usage\r\n> ", Output());
}

TEST_F(HilTerminalTest, unknown_command_answers_usage)
{
    Output();

    Receive("foo bar\r");

    EXPECT_EQ("foo bar\r\nERR usage\r\n> ", Output());
}

TEST_F(HilTerminalTest, line_after_command_starts_on_new_line)
{
    Receive("ping\r");
    Output();

    response.Ok();

    EXPECT_EQ("\r\nOK\r\n", Output());
}
