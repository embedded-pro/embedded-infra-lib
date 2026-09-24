#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/fsm/StateTimeouts.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "services/fsm/test_doubles/StateMachineObserverMock.hpp"
#include "services/fsm/test_doubles/UncheckedTableStateMachine.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>

namespace
{
    struct Resting
    {
        static constexpr const char* name{ "Resting" };
    };

    struct Waiting
    {
        static constexpr const char* name{ "Waiting" };
    };

    struct Done
    {
        static constexpr const char* name{ "Done" };
    };

    using State = std::variant<Resting, Waiting, Done>;

    struct Begin
    {
        static constexpr const char* name{ "Begin" };
    };

    struct Expired
    {
        static constexpr const char* name{ "Expired" };
    };

    struct Finish
    {
        static constexpr const char* name{ "Finish" };
    };

    using Event = std::variant<Begin, Expired, Finish>;

    struct Job
    {};

    using StateId = services::AlternativeId<State>;
    using Timeout = services::StateTimeout<State, Event>;
    using Machine = services::TableStateMachine<State, Event, Job>;
    using Unchecked = services::UncheckedTableStateMachine<State, Event, Job>;

    constexpr std::array<Timeout, 2> timeouts{ {
        { StateId::Of<Waiting>(), std::chrono::seconds(10), Event{ Expired{} } },
        { StateId::Of<Done>(), std::chrono::seconds(2), Event{ Begin{} } },
    } };

    constexpr std::array rows{
        Machine::Row<Resting, Begin, Waiting>(),
        Machine::Row<Waiting, Expired, Resting>(),
        Machine::Row<Waiting, Finish, Done>(),
        Machine::Row<Done, Begin, Waiting>(),
        Machine::InternalRow<Waiting, Begin>(),
    };

    template<class T>
    StateId Id()
    {
        return StateId::Of<T>();
    }
}

class StateTimeoutsTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    Job job;
    Unchecked::WithStorage<2> fsm{ job, infra::MakeRange(rows) };
    services::StateTimeouts<State, Event> stateTimeouts{ fsm, infra::MakeRange(timeouts) };
    testing::StrictMock<services::StateMachineObserverMock<State, Event>> observer{ fsm };
};

TEST_F(StateTimeoutsTest, state_without_timeout_does_not_arm_timer)
{
    EXPECT_CALL(observer, Started(Id<Resting>()));
    fsm.Start<Resting>();

    ForwardTime(std::chrono::hours(1));

    EXPECT_TRUE(fsm.Is<Resting>());
}

TEST_F(StateTimeoutsTest, timeout_dispatches_configured_event_after_duration)
{
    EXPECT_CALL(observer, Started(Id<Resting>()));
    fsm.Start<Resting>();
    EXPECT_CALL(observer, StateChanged(Id<Resting>(), testing::_, Id<Waiting>()));
    fsm.Dispatch(Begin{});

    ForwardTime(std::chrono::seconds(9));
    EXPECT_TRUE(fsm.Is<Waiting>());

    EXPECT_CALL(observer, StateChanged(Id<Waiting>(), testing::VariantWith<Expired>(testing::_), Id<Resting>()));
    ForwardTime(std::chrono::seconds(1));
    EXPECT_TRUE(fsm.Is<Resting>());
}

TEST_F(StateTimeoutsTest, leaving_state_before_timeout_cancels_timer)
{
    EXPECT_CALL(observer, Started(Id<Resting>()));
    fsm.Start<Resting>();
    EXPECT_CALL(observer, StateChanged(Id<Resting>(), testing::_, Id<Waiting>()));
    fsm.Dispatch(Begin{});

    ForwardTime(std::chrono::seconds(5));
    EXPECT_CALL(observer, StateChanged(Id<Waiting>(), testing::_, Id<Done>()));
    fsm.Dispatch(Finish{});

    EXPECT_CALL(observer, StateChanged(Id<Done>(), testing::VariantWith<Begin>(testing::_), Id<Waiting>()));
    ForwardTime(std::chrono::seconds(2));
    EXPECT_TRUE(fsm.Is<Waiting>());
}

TEST_F(StateTimeoutsTest, re_entering_state_restarts_timeout)
{
    EXPECT_CALL(observer, Started(Id<Resting>()));
    fsm.Start<Resting>();
    EXPECT_CALL(observer, StateChanged(Id<Resting>(), testing::_, Id<Waiting>()));
    fsm.Dispatch(Begin{});

    ForwardTime(std::chrono::seconds(8));
    EXPECT_CALL(observer, StateChanged(Id<Waiting>(), testing::_, Id<Done>()));
    fsm.Dispatch(Finish{});
    EXPECT_CALL(observer, StateChanged(Id<Done>(), testing::_, Id<Waiting>()));
    fsm.Dispatch(Begin{});

    ForwardTime(std::chrono::seconds(9));
    EXPECT_TRUE(fsm.Is<Waiting>());

    EXPECT_CALL(observer, StateChanged(Id<Waiting>(), testing::VariantWith<Expired>(testing::_), Id<Resting>()));
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(StateTimeoutsTest, internal_transition_does_not_restart_timeout)
{
    EXPECT_CALL(observer, Started(Id<Resting>()));
    fsm.Start<Resting>();
    EXPECT_CALL(observer, StateChanged(Id<Resting>(), testing::_, Id<Waiting>()));
    fsm.Dispatch(Begin{});

    ForwardTime(std::chrono::seconds(8));
    EXPECT_CALL(observer, EventHandled(Id<Waiting>(), testing::VariantWith<Begin>(testing::_)));
    fsm.Dispatch(Begin{});

    EXPECT_CALL(observer, StateChanged(Id<Waiting>(), testing::VariantWith<Expired>(testing::_), Id<Resting>()));
    ForwardTime(std::chrono::seconds(2));
}

TEST_F(StateTimeoutsTest, initial_state_timeout_is_armed_on_start)
{
    EXPECT_CALL(observer, Started(Id<Waiting>()));
    fsm.Start<Waiting>();

    EXPECT_CALL(observer, StateChanged(Id<Waiting>(), testing::VariantWith<Expired>(testing::_), Id<Resting>()));
    ForwardTime(std::chrono::seconds(10));
}
