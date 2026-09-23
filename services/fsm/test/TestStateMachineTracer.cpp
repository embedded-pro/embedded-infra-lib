#include "infra/stream/StringOutputStream.hpp"
#include "services/fsm/StateMachineTracer.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "services/fsm/test_doubles/UncheckedTableStateMachine.hpp"
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

    struct Door
    {
        bool allowPull{ true };
    };

    using Machine = services::TableStateMachine<State, Event, Door>;
    using Unchecked = services::UncheckedTableStateMachine<State, Event, Door>;

    constexpr std::array rows{
        Machine::Row<Closed, Push, Open>(),
        Machine::Row<Open, Pull, Closed>([](Door& door, const Open&, const Pull&)
            {
                return door.allowPull;
            }),
        Machine::InternalRow<Open, Push>(),
    };

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
    infra::StringOutputStream::WithStorage<128> stream;
    TracerToStreamWithoutHeader tracer{ stream };
    Door door;
    Unchecked::WithStorage<2> fsm{ door, infra::MakeRange(rows) };
    services::StateMachineTracer<State, Event> stateMachineTracer{ fsm, tracer };
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
    door.allowPull = false;
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
