#include "infra/stream/StringOutputStream.hpp"
#include "services/fsm/StateMachineTracer.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "services/tracer/Tracer.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    struct Closed
    {
        static constexpr const char* name{ "Closed" };
    };

    struct Open
    {
        static constexpr const char* name{ "Open" };
    };

    using State = std::variant<Closed, Open>;

    struct Push
    {
        static constexpr const char* name{ "Push" };
    };

    struct Pull
    {
        static constexpr const char* name{ "Pull" };
    };

    using Event = std::variant<Push, Pull>;

    class TracerToStreamWithoutHeader
        : public services::TracerToStream
    {
    public:
        using services::TracerToStream::TracerToStream;

    protected:
        void InsertHeader() override
        {}
    };
}

class StateMachineTracerTest
    : public testing::Test
{
public:
    StateMachineTracerTest()
    {
        fsm.Add<Closed, Push, Open>(nullptr, nullptr)
            .Add<Open, Pull, Closed>([this](const Open&, const Pull&)
                {
                    return allowPull;
                })
            .AddInternal<Open, Push>();
    }

    infra::StringOutputStream::WithStorage<128> stream;
    TracerToStreamWithoutHeader tracer{ stream };
    services::TableStateMachine<State, Event>::WithStorage<4, 2> fsm;
    services::StateMachineTracer<State, Event> stateMachineTracer{ fsm, tracer };
    bool allowPull{ true };
};

TEST_F(StateMachineTracerTest, start_is_traced)
{
    fsm.Start<Closed>();

    EXPECT_EQ("\r\nfsm: started in Closed", stream.Storage());
}

TEST_F(StateMachineTracerTest, transition_is_traced)
{
    fsm.Start<Closed>();
    stream.Storage().clear();

    fsm.Dispatch(Push{});

    EXPECT_EQ("\r\nfsm: Closed --Push--> Open", stream.Storage());
}

TEST_F(StateMachineTracerTest, forbidden_event_is_traced)
{
    fsm.Start<Closed>();
    stream.Storage().clear();

    fsm.Dispatch(Pull{});

    EXPECT_EQ("\r\nfsm: forbidden Pull in Closed", stream.Storage());
}

TEST_F(StateMachineTracerTest, rejected_event_is_traced)
{
    fsm.Start<Open>();
    allowPull = false;
    stream.Storage().clear();

    fsm.Dispatch(Pull{});

    EXPECT_EQ("\r\nfsm: rejected Pull in Open", stream.Storage());
}

TEST_F(StateMachineTracerTest, discarded_completion_is_traced)
{
    fsm.Start<Closed>();
    auto done = fsm.Completion<Pull>();
    fsm.Dispatch(Push{});
    stream.Storage().clear();

    done();

    EXPECT_EQ("\r\nfsm: discarded Pull in Open", stream.Storage());
}

TEST_F(StateMachineTracerTest, internal_transition_is_traced)
{
    fsm.Start<Open>();
    stream.Storage().clear();

    fsm.Dispatch(Push{});

    EXPECT_EQ("\r\nfsm: handled Push in Open", stream.Storage());
}
