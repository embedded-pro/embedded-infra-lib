#include "services/fsm/TableStateMachine.hpp"
#include "services/fsm/test_doubles/StateMachineObserverMock.hpp"
#include "services/fsm/test_doubles/UncheckedTableStateMachine.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <optional>

namespace
{
    class Hooks
    {
    public:
        MOCK_METHOD(void, Entry, (const char* state));
        MOCK_METHOD(void, Exit, (const char* state));
        MOCK_METHOD(void, Action, (const char* action));
        MOCK_METHOD(bool, Guard, (const char* guard));
    };

    struct Off;
    struct On;
    struct Broken;
    struct Locked;
    using State = std::variant<Off, On, Broken, Locked>;

    struct Press
    {
        static constexpr const char* name{ "Press" };
    };

    struct Dim
    {
        static constexpr const char* name{ "Dim" };
        int amount{ 0 };
    };

    struct Break
    {
        static constexpr const char* name{ "Break" };
        int code{ 0 };
    };

    struct Fix
    {
        static constexpr const char* name{ "Fix" };
    };

    struct Lock
    {
        static constexpr const char* name{ "Lock" };
    };

    using Event = std::variant<Press, Dim, Break, Fix, Lock>;

    struct Off
    {
        static constexpr const char* name{ "Off" };
    };

    struct On
    {
        static constexpr const char* name{ "On" };

        void OnEntry()
        {
            hooks->Entry("On");
        }

        void OnExit()
        {
            hooks->Exit("On");
        }

        Hooks* hooks{ nullptr };
        int level{ 0 };
    };

    struct Broken
    {
        static constexpr const char* name{ "Broken" };
        int code{ 0 };
    };

    struct Locked
    {
        static constexpr const char* name{ "Locked" };

        void OnEntry()
        {
            hooks.Entry(machine.Is<Locked>() ? "Locked committed" : "Locked uncommitted");
        }

        services::StateMachine<State, Event>& machine;
        Hooks& hooks;
    };

    struct Lamp;

    using StateId = services::AlternativeId<State>;
    using Machine = services::TableStateMachine<State, Event, Lamp>;
    using Unchecked = services::UncheckedTableStateMachine<State, Event, Lamp>;

    struct Lamp
    {
        Hooks& hooks;
        Machine* fsm{ nullptr };
        std::optional<services::DispatchResult> nestedResult;
        infra::Function<void()> done;
    };

    template<class T>
    StateId Id()
    {
        return StateId::Of<T>();
    }

    template<class E>
    auto With()
    {
        return testing::VariantWith<E>(testing::_);
    }

    constexpr std::array common{
        Machine::Row<On, Press, Off>(),
        Machine::RowFromAny<Break, Broken>(nullptr, [](Lamp&, State&, const Break& event)
            {
                return Broken{ event.code };
            }),
        Machine::Row<Broken, Fix, Off>(),
        Machine::Row<Off, Lock, Locked>(nullptr, [](Lamp& lamp, Off&, const Lock&)
            {
                return Locked{ *lamp.fsm, lamp.hooks };
            }),
        Machine::Row<Locked, Fix, Off>(),
    };

    constexpr std::array offToOn{
        Machine::Row<Off, Press, On>(nullptr, [](Lamp& lamp, Off&, const Press&)
            {
                return On{ &lamp.hooks };
            }),
    };

    constexpr auto baseline = services::JoinRows(common, offToOn);
    constexpr std::array<Machine::Transition, 0> empty{};
}

class TableStateMachineTest
    : public testing::Test
{
public:
    void Build(Machine::Table table)
    {
        observer.reset();
        fsm.emplace(lamp, table);
        lamp.fsm = &*fsm;
        observer.emplace(*fsm);
    }

    template<class Initial, class... Args>
    void Start(Args&&... args)
    {
        if constexpr (std::is_same_v<Initial, On>)
            EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
        if constexpr (std::is_same_v<Initial, Locked>)
            EXPECT_CALL(hooks, Entry(testing::StrEq("Locked committed")));
        EXPECT_CALL(*observer, Started(Id<Initial>()));
        fsm->Start<Initial>(std::forward<Args>(args)...);
    }

    testing::StrictMock<Hooks> hooks;
    Lamp lamp{ hooks };
    std::optional<Unchecked::WithStorage<4>> fsm;
    std::optional<testing::StrictMock<services::StateMachineObserverMock<State, Event>>> observer;
};

TEST_F(TableStateMachineTest, valid_table_has_no_consistency_error)
{
    Build(infra::MakeRange(baseline));

    EXPECT_EQ(services::ConsistencyError::none, fsm->CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, empty_table_is_rejected)
{
    Build(infra::MakeRange(empty));

    EXPECT_EQ(services::ConsistencyError::emptyTable, fsm->CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, two_unguarded_rows_for_same_pair_are_duplicate)
{
    static constexpr auto table = services::JoinRows(baseline, std::array{ Machine::Row<Off, Press, Broken>() });
    Build(infra::MakeRange(table));

    EXPECT_EQ(services::ConsistencyError::duplicateTransition, fsm->CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, row_after_unguarded_row_is_shadowed)
{
    static constexpr auto table = services::JoinRows(baseline, std::array{ Machine::Row<Off, Press, Broken>([](Lamp&, const Off&, const Press&)
                                                                   {
                                                                       return true;
                                                                   }) });
    Build(infra::MakeRange(table));

    EXPECT_EQ(services::ConsistencyError::shadowedTransition, fsm->CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, guarded_rows_for_same_pair_are_allowed)
{
    static constexpr auto table = services::JoinRows(common, std::array{
                                                                 Machine::Row<Off, Press, On>([](Lamp&, const Off&, const Press&)
                                                                     {
                                                                         return true;
                                                                     },
                                                                     [](Lamp& lamp, Off&, const Press&)
                                                                     {
                                                                         return On{ &lamp.hooks };
                                                                     }),
                                                                 Machine::Row<Off, Press, Broken>(),
                                                             });
    Build(infra::MakeRange(table));

    EXPECT_EQ(services::ConsistencyError::none, fsm->CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, state_class_not_reachable_from_initial_is_rejected)
{
    Build(infra::MakeRange(offToOn));

    EXPECT_EQ(services::ConsistencyError::unreachableState, fsm->CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, any_state_row_makes_target_reachable)
{
    static constexpr auto withoutFallback = services::JoinRows(offToOn, std::array{ Machine::Row<Off, Lock, Locked>(nullptr, [](Lamp& lamp, Off&, const Lock&)
                                                                            {
                                                                                return Locked{ *lamp.fsm, lamp.hooks };
                                                                            }) });
    static constexpr auto withFallback = services::JoinRows(withoutFallback, std::array{ Machine::RowFromAny<Break, Broken>() });

    Build(infra::MakeRange(withoutFallback));
    EXPECT_EQ(services::ConsistencyError::unreachableState, fsm->CheckConsistency<Off>());

    Build(infra::MakeRange(withFallback));
    EXPECT_EQ(services::ConsistencyError::none, fsm->CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, has_transition_reports_specific_and_wildcard_rows)
{
    Build(infra::MakeRange(baseline));

    EXPECT_TRUE((fsm->HasTransition<Off, Press>()));
    EXPECT_TRUE((fsm->HasTransition<On, Break>()));
    EXPECT_FALSE((fsm->HasTransition<On, Fix>()));
    EXPECT_TRUE(fsm->HasTransition(Id<Locked>(), services::AlternativeId<Event>::Of<Fix>()));
}

TEST_F(TableStateMachineTest, transitions_exposes_rows_in_table_order)
{
    Build(infra::MakeRange(baseline));

    auto rows = fsm->Transitions();
    ASSERT_EQ(6, rows.size());
    EXPECT_EQ(Id<On>().Index(), *rows[0].from);
    EXPECT_FALSE(rows[1].from.has_value());
    EXPECT_EQ(Id<Broken>().Index(), rows[1].to);
    EXPECT_EQ(baseline.data(), rows.begin());
}

TEST_F(TableStateMachineTest, start_with_inconsistent_table_asserts)
{
    Build(infra::MakeRange(empty));

    EXPECT_DEATH(fsm->Start<Off>(), "");
}

TEST_F(TableStateMachineTest, start_constructs_initial_state_runs_its_entry_and_notifies_started)
{
    Build(infra::MakeRange(baseline));

    Start<On>(On{ &hooks, 4 });

    EXPECT_TRUE(fsm->Started());
    EXPECT_TRUE(fsm->Is<On>());
    EXPECT_EQ(4, std::get<On>(fsm->CurrentState()).level);
    EXPECT_EQ(Id<On>(), fsm->CurrentStateId());
}

TEST_F(TableStateMachineTest, dispatch_before_start_asserts)
{
    Build(infra::MakeRange(baseline));

    EXPECT_FALSE(fsm->Started());
    EXPECT_DEATH(fsm->Dispatch(Press{}), "");
}

TEST_F(TableStateMachineTest, matching_row_runs_exit_action_entry_then_notifies_in_order)
{
    static constexpr auto table = services::JoinRows(common, std::array{ Machine::Row<Off, Press, On>(nullptr, [](Lamp& lamp, Off&, const Press&)
                                                                 {
                                                                     lamp.hooks.Action("build On");
                                                                     return On{ &lamp.hooks };
                                                                 }) });
    Build(infra::MakeRange(table));
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Action(testing::StrEq("build On")));
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_EQ(services::DispatchResult::transitioned, fsm->Dispatch(Press{}));

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), With<Press>(), Id<Off>()));
    EXPECT_EQ(services::DispatchResult::transitioned, fsm->Dispatch(Press{}));
    EXPECT_TRUE(fsm->Is<Off>());
}

TEST_F(TableStateMachineTest, action_receives_context_typed_source_state_and_event_and_builds_target)
{
    static constexpr auto table = services::JoinRows(common, std::array{ Machine::Row<Off, Press, On>(nullptr, [](Lamp& lamp, Off&, const Press&)
                                                                 {
                                                                     return On{ &lamp.hooks, 3 };
                                                                 }) });
    Build(infra::MakeRange(table));
    Start<Off>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm->Dispatch(Press{});
    EXPECT_EQ(3, std::get<On>(fsm->CurrentState()).level);

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), testing::VariantWith<Break>(testing::Field(&Break::code, 7)), Id<Broken>()));
    fsm->Dispatch(Break{ 7 });
    EXPECT_EQ(7, std::get<Broken>(fsm->CurrentState()).code);
}

TEST_F(TableStateMachineTest, omitted_action_default_constructs_target)
{
    Build(infra::MakeRange(baseline));
    Start<Broken>(Broken{ 2 });

    EXPECT_CALL(*observer, StateChanged(Id<Broken>(), With<Fix>(), Id<Off>()));
    fsm->Dispatch(Fix{});

    EXPECT_TRUE(fsm->Is<Off>());
}

TEST_F(TableStateMachineTest, entry_runs_on_committed_state)
{
    Build(infra::MakeRange(baseline));
    Start<Off>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("Locked committed")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Lock>(), Id<Locked>()));
    fsm->Dispatch(Lock{});
}

TEST_F(TableStateMachineTest, event_without_row_is_forbidden_and_notifies_without_changing_state)
{
    Build(infra::MakeRange(baseline));
    Start<Off>();

    EXPECT_CALL(*observer, EventForbidden(Id<Off>(), With<Fix>()));
    EXPECT_EQ(services::DispatchResult::forbidden, fsm->Dispatch(Fix{}));

    EXPECT_TRUE(fsm->Is<Off>());
}

TEST_F(TableStateMachineTest, event_whose_guards_all_refuse_is_rejected_and_notifies)
{
    static constexpr auto table = services::JoinRows(common, std::array{ Machine::Row<Off, Press, On>([](Lamp& lamp, const Off&, const Press&)
                                                                 {
                                                                     return lamp.hooks.Guard("allow");
                                                                 },
                                                                 [](Lamp& lamp, Off&, const Press&)
                                                                 {
                                                                     return On{ &lamp.hooks };
                                                                 }) });
    Build(infra::MakeRange(table));
    Start<Off>();

    EXPECT_CALL(hooks, Guard(testing::StrEq("allow"))).WillOnce(testing::Return(false));
    EXPECT_CALL(*observer, EventRejected(Id<Off>(), With<Press>()));
    EXPECT_EQ(services::DispatchResult::rejected, fsm->Dispatch(Press{}));

    EXPECT_TRUE(fsm->Is<Off>());
}

TEST_F(TableStateMachineTest, first_row_whose_guard_accepts_wins_and_later_guards_are_not_evaluated)
{
    static constexpr auto table = services::JoinRows(common, std::array{
                                                                 Machine::Row<Off, Press, On>([](Lamp& lamp, const Off&, const Press&)
                                                                     {
                                                                         return lamp.hooks.Guard("first");
                                                                     },
                                                                     [](Lamp& lamp, Off&, const Press&)
                                                                     {
                                                                         return On{ &lamp.hooks };
                                                                     }),
                                                                 Machine::Row<Off, Press, Broken>([](Lamp& lamp, const Off&, const Press&)
                                                                     {
                                                                         return lamp.hooks.Guard("second");
                                                                     }),
                                                             });
    Build(infra::MakeRange(table));
    Start<Off>();

    EXPECT_CALL(hooks, Guard(testing::StrEq("first"))).WillOnce(testing::Return(false));
    EXPECT_CALL(hooks, Guard(testing::StrEq("second"))).WillOnce(testing::Return(true));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<Broken>()));
    fsm->Dispatch(Press{});

    EXPECT_CALL(*observer, StateChanged(Id<Broken>(), With<Fix>(), Id<Off>()));
    fsm->Dispatch(Fix{});

    EXPECT_CALL(hooks, Guard(testing::StrEq("first"))).WillOnce(testing::Return(true));
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm->Dispatch(Press{});
}

TEST_F(TableStateMachineTest, specific_row_beats_any_state_row)
{
    static constexpr auto table = services::JoinRows(baseline, std::array{ Machine::Row<On, Break, Off>() });
    Build(infra::MakeRange(table));
    Start<On>(On{ &hooks });

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), With<Break>(), Id<Off>()));
    fsm->Dispatch(Break{});
    EXPECT_TRUE(fsm->Is<Off>());

    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Break>(), Id<Broken>()));
    fsm->Dispatch(Break{});
    EXPECT_TRUE(fsm->Is<Broken>());
}

TEST_F(TableStateMachineTest, any_state_row_is_fallback_when_specific_guard_refuses)
{
    static constexpr auto table = services::JoinRows(baseline, std::array{ Machine::Row<On, Break, Off>([](Lamp& lamp, const On&, const Break&)
                                                                   {
                                                                       return lamp.hooks.Guard("specific");
                                                                   }) });
    Build(infra::MakeRange(table));
    Start<On>(On{ &hooks });

    EXPECT_CALL(hooks, Guard(testing::StrEq("specific"))).WillOnce(testing::Return(false));
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), With<Break>(), Id<Broken>()));
    fsm->Dispatch(Break{});
}

TEST_F(TableStateMachineTest, any_state_row_fires_from_every_state_with_event_payload)
{
    Build(infra::MakeRange(baseline));
    Start<Off>();

    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Lock>(), Id<Locked>()));
    EXPECT_CALL(hooks, Entry(testing::StrEq("Locked committed")));
    fsm->Dispatch(Lock{});

    EXPECT_CALL(*observer, StateChanged(Id<Locked>(), With<Break>(), Id<Broken>()));
    fsm->Dispatch(Break{ 5 });
    EXPECT_EQ(5, std::get<Broken>(fsm->CurrentState()).code);

    EXPECT_CALL(*observer, StateChanged(Id<Broken>(), With<Break>(), Id<Broken>()));
    fsm->Dispatch(Break{ 6 });
    EXPECT_EQ(6, std::get<Broken>(fsm->CurrentState()).code);
}

TEST_F(TableStateMachineTest, internal_row_runs_action_without_exit_or_entry)
{
    static constexpr auto table = services::JoinRows(baseline, std::array{ Machine::InternalRow<On, Dim>([](Lamp& lamp, On& state, const Dim& event)
                                                                   {
                                                                       lamp.hooks.Action("dim");
                                                                       state.level += event.amount;
                                                                   }) });
    Build(infra::MakeRange(table));
    Start<On>(On{ &hooks, 10 });

    EXPECT_CALL(hooks, Action(testing::StrEq("dim")));
    EXPECT_CALL(*observer, EventHandled(Id<On>(), With<Dim>()));
    EXPECT_EQ(services::DispatchResult::transitioned, fsm->Dispatch(Dim{ -3 }));

    EXPECT_EQ(7, std::get<On>(fsm->CurrentState()).level);
}

namespace
{
    constexpr auto baselineWithDimInOff = services::JoinRows(baseline, std::array{ Machine::InternalRow<Off, Dim>() });
}

TEST_F(TableStateMachineTest, internal_row_without_action_ignores_event_and_notifies_event_handled)
{
    Build(infra::MakeRange(baselineWithDimInOff));
    Start<Off>();

    EXPECT_CALL(*observer, EventHandled(Id<Off>(), With<Dim>()));
    EXPECT_EQ(services::DispatchResult::transitioned, fsm->Dispatch(Dim{ 1 }));

    EXPECT_TRUE(fsm->Is<Off>());
}

TEST_F(TableStateMachineTest, guarded_internal_row_is_rejected_when_guard_refuses)
{
    static constexpr auto table = services::JoinRows(baseline, std::array{ Machine::InternalRow<Off, Dim>(nullptr, [](Lamp& lamp, const Off&, const Dim&)
                                                                   {
                                                                       return lamp.hooks.Guard("dim");
                                                                   }) });
    Build(infra::MakeRange(table));
    Start<Off>();

    EXPECT_CALL(hooks, Guard(testing::StrEq("dim"))).WillOnce(testing::Return(false));
    EXPECT_CALL(*observer, EventRejected(Id<Off>(), With<Dim>()));
    EXPECT_EQ(services::DispatchResult::rejected, fsm->Dispatch(Dim{ 1 }));
}

TEST_F(TableStateMachineTest, external_self_transition_runs_exit_and_entry)
{
    static constexpr auto table = services::JoinRows(baseline, std::array{ Machine::Row<On, Lock, On>(nullptr, [](Lamp& lamp, On& from, const Lock&)
                                                                   {
                                                                       return On{ &lamp.hooks, from.level + 1 };
                                                                   }) });
    Build(infra::MakeRange(table));
    Start<On>(On{ &hooks, 1 });

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), With<Lock>(), Id<On>()));
    fsm->Dispatch(Lock{});

    EXPECT_EQ(2, std::get<On>(fsm->CurrentState()).level);
}

TEST_F(TableStateMachineTest, state_with_reference_member_can_self_transition)
{
    static constexpr auto table = services::JoinRows(baseline, std::array{ Machine::Row<Locked, Lock, Locked>(nullptr, [](Lamp& lamp, Locked&, const Lock&)
                                                                   {
                                                                       return Locked{ *lamp.fsm, lamp.hooks };
                                                                   }) });
    Build(infra::MakeRange(table));
    Start<Locked>(Locked{ *fsm, hooks });

    EXPECT_CALL(hooks, Entry(testing::StrEq("Locked committed")));
    EXPECT_CALL(*observer, StateChanged(Id<Locked>(), With<Lock>(), Id<Locked>()));
    fsm->Dispatch(Lock{});
}

TEST_F(TableStateMachineTest, dispatch_from_action_is_queued_and_handled_after_commit)
{
    static constexpr auto table = services::JoinRows(common, std::array{ Machine::Row<Off, Press, On>(nullptr, [](Lamp& lamp, Off&, const Press&)
                                                                 {
                                                                     lamp.nestedResult = lamp.fsm->Dispatch(Press{});
                                                                     return On{ &lamp.hooks };
                                                                 }) });
    Build(infra::MakeRange(table));
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), With<Press>(), Id<Off>()));
    EXPECT_EQ(services::DispatchResult::transitioned, fsm->Dispatch(Press{}));

    EXPECT_EQ(services::DispatchResult::queued, lamp.nestedResult);
    EXPECT_TRUE(fsm->Is<Off>());
}

TEST_F(TableStateMachineTest, dispatch_from_observer_is_queued)
{
    Build(infra::MakeRange(baseline));
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>())).WillOnce(testing::Invoke([this]()
        {
            EXPECT_EQ(services::DispatchResult::queued, fsm->Dispatch(Break{ 1 }));
        }));
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), With<Break>(), Id<Broken>()));
    fsm->Dispatch(Press{});

    EXPECT_TRUE(fsm->Is<Broken>());
}

TEST_F(TableStateMachineTest, queued_events_are_handled_in_order)
{
    static constexpr auto table = services::JoinRows(common, std::array{ Machine::Row<Off, Press, On>(nullptr, [](Lamp& lamp, Off&, const Press&)
                                                                 {
                                                                     lamp.fsm->Dispatch(Break{ 1 });
                                                                     lamp.fsm->Dispatch(Fix{});
                                                                     return On{ &lamp.hooks };
                                                                 }) });
    Build(infra::MakeRange(table));
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), With<Break>(), Id<Broken>()));
    EXPECT_CALL(*observer, StateChanged(Id<Broken>(), With<Fix>(), Id<Off>()));
    fsm->Dispatch(Press{});

    EXPECT_TRUE(fsm->Is<Off>());
}

TEST_F(TableStateMachineTest, dispatch_during_start_is_queued_until_initial_state_is_committed)
{
    Build(infra::MakeRange(baseline));

    EXPECT_CALL(*observer, Started(Id<Off>())).WillOnce(testing::Invoke([this]()
        {
            EXPECT_EQ(services::DispatchResult::queued, fsm->Dispatch(Press{}));
        }));
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm->Start<Off>();

    EXPECT_TRUE(fsm->Is<On>());
}

namespace
{
    constexpr std::array overflowing{
        Machine::Row<Off, Press, Off>(nullptr, [](Lamp& lamp, Off&, const Press&)
            {
                lamp.fsm->Dispatch(Press{});
                lamp.fsm->Dispatch(Press{});
                return Off{};
            }),
        Machine::Row<Off, Fix, On>(),
        Machine::RowFromAny<Break, Broken>(),
        Machine::Row<Off, Lock, Locked>(nullptr, [](Lamp&, Off&, const Lock&) -> Locked
            {
                std::abort();
            }),
    };

    void OverflowQueue()
    {
        testing::StrictMock<Hooks> hooks;
        Lamp lamp{ hooks };
        Unchecked::WithStorage<1> fsm{ lamp, infra::MakeRange(overflowing) };
        lamp.fsm = &fsm;
        fsm.Start<Off>();
        fsm.Dispatch(Press{});
    }
}

TEST(TableStateMachineDeathTest, queue_overflow_asserts)
{
    EXPECT_DEATH(OverflowQueue(), "");
}

TEST_F(TableStateMachineTest, completion_dispatches_when_epoch_unchanged)
{
    Build(infra::MakeRange(baseline));
    Start<Off>();

    auto done = fsm->Completion<Press>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    done();

    EXPECT_TRUE(fsm->Is<On>());
}

TEST_F(TableStateMachineTest, completion_after_transition_is_discarded_and_notifies)
{
    Build(infra::MakeRange(baseline));
    Start<Off>();
    auto done = fsm->Completion<Press>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm->Dispatch(Press{});

    EXPECT_CALL(*observer, EventDiscarded(Id<On>(), With<Press>()));
    done();

    EXPECT_TRUE(fsm->Is<On>());
}

TEST_F(TableStateMachineTest, completion_after_self_transition_is_discarded)
{
    static constexpr auto table = services::JoinRows(baseline, std::array{ Machine::Row<On, Lock, On>(nullptr, [](Lamp& lamp, On&, const Lock&)
                                                                   {
                                                                       return On{ &lamp.hooks };
                                                                   }) });
    Build(infra::MakeRange(table));
    Start<On>(On{ &hooks });
    auto done = fsm->Completion<Press>();

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), With<Lock>(), Id<On>()));
    fsm->Dispatch(Lock{});

    EXPECT_CALL(*observer, EventDiscarded(Id<On>(), With<Press>()));
    done();
}

TEST_F(TableStateMachineTest, completion_created_in_action_belongs_to_target_state)
{
    static constexpr auto table = services::JoinRows(common, std::array{ Machine::Row<Off, Press, On>(nullptr, [](Lamp& lamp, Off&, const Press&)
                                                                 {
                                                                     lamp.done = lamp.fsm->Completion<Press>();
                                                                     return On{ &lamp.hooks };
                                                                 }) });
    Build(infra::MakeRange(table));
    Start<Off>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm->Dispatch(Press{});

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), With<Press>(), Id<Off>()));
    lamp.done();

    EXPECT_TRUE(fsm->Is<Off>());
}

TEST_F(TableStateMachineTest, completion_is_not_discarded_by_internal_transition)
{
    Build(infra::MakeRange(baselineWithDimInOff));
    Start<Off>();
    auto done = fsm->Completion<Press>();

    EXPECT_CALL(*observer, EventHandled(Id<Off>(), With<Dim>()));
    fsm->Dispatch(Dim{});

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    done();
}

TEST_F(TableStateMachineTest, completion_with_payload_carries_payload)
{
    Build(infra::MakeRange(baseline));
    Start<Off>();

    auto done = fsm->Completion<Break, 4 * sizeof(void*)>(Break{ 9 });

    EXPECT_CALL(*observer, StateChanged(Id<Off>(), testing::VariantWith<Break>(testing::Field(&Break::code, 9)), Id<Broken>()));
    done();

    EXPECT_EQ(9, std::get<Broken>(fsm->CurrentState()).code);
}

TEST_F(TableStateMachineTest, observer_may_detach_during_notification)
{
    Build(infra::MakeRange(baseline));
    testing::StrictMock<services::StateMachineObserverMock<State, Event>> second{ *fsm };
    EXPECT_CALL(second, Started(Id<Off>()));
    Start<Off>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_CALL(second, StateChanged(Id<Off>(), With<Press>(), Id<On>())).WillOnce(testing::Invoke([&second]()
        {
            second.Detach();
        }));
    fsm->Dispatch(Press{});

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), With<Press>(), Id<Off>()));
    fsm->Dispatch(Press{});
}

TEST_F(TableStateMachineTest, current_state_is_readable_while_dispatching)
{
    static constexpr auto table = services::JoinRows(common, std::array{ Machine::Row<Off, Press, On>(nullptr, [](Lamp& lamp, Off&, const Press&)
                                                                 {
                                                                     EXPECT_TRUE(lamp.fsm->Dispatching());
                                                                     EXPECT_TRUE(lamp.fsm->Is<Off>());
                                                                     return On{ &lamp.hooks };
                                                                 }) });
    Build(infra::MakeRange(table));
    Start<Off>();
    EXPECT_FALSE(fsm->Dispatching());

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm->Dispatch(Press{});

    EXPECT_FALSE(fsm->Dispatching());
}

TEST_F(TableStateMachineTest, on_entered_hook_runs_after_observers_with_the_committed_state)
{
    Build(infra::MakeRange(baseline));
    fsm->OnEntered<On>([](Lamp& lamp, On& state)
        {
            lamp.hooks.Action("entered On");
            state.level = 9;
        });
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_CALL(hooks, Action(testing::StrEq("entered On")));
    fsm->Dispatch(Press{});

    EXPECT_EQ(9, std::get<On>(fsm->CurrentState()).level);
}

TEST_F(TableStateMachineTest, on_entered_hook_runs_for_initial_state_on_start)
{
    Build(infra::MakeRange(baseline));
    fsm->OnEntered<Off>([](Lamp& lamp, Off&)
        {
            lamp.hooks.Action("entered Off");
        });

    testing::InSequence sequence;
    EXPECT_CALL(*observer, Started(Id<Off>()));
    EXPECT_CALL(hooks, Action(testing::StrEq("entered Off")));
    fsm->Start<Off>();
}

TEST_F(TableStateMachineTest, internal_row_does_not_run_on_entered_hook)
{
    Build(infra::MakeRange(baselineWithDimInOff));
    fsm->OnEntered<Off>([](Lamp& lamp, Off&)
        {
            lamp.hooks.Action("entered Off");
        });
    EXPECT_CALL(hooks, Action(testing::StrEq("entered Off")));
    Start<Off>();

    EXPECT_CALL(*observer, EventHandled(Id<Off>(), With<Dim>()));
    fsm->Dispatch(Dim{});
}

TEST_F(TableStateMachineTest, dispatch_from_on_entered_hook_is_queued_and_handled_afterwards)
{
    Build(infra::MakeRange(baseline));
    fsm->OnEntered<On>([](Lamp& lamp, On&)
        {
            EXPECT_EQ(services::DispatchResult::queued, lamp.fsm->Dispatch(Press{}));
        });
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<On>(), With<Press>(), Id<Off>()));
    fsm->Dispatch(Press{});

    EXPECT_TRUE(fsm->Is<Off>());
}

TEST_F(TableStateMachineTest, on_entered_after_start_asserts)
{
    Build(infra::MakeRange(baseline));
    Start<Off>();

    EXPECT_DEATH((fsm->OnEntered<Off>([](Lamp&, Off&) {})), "");
}

TEST_F(TableStateMachineTest, completion_with_maps_callback_arguments_to_event)
{
    Build(infra::MakeRange(baseline));
    Start<Off>();

    auto done = fsm->CompletionWith<void(int)>([](int code)
        {
            return Break{ code };
        });

    EXPECT_CALL(*observer, StateChanged(Id<Off>(), testing::VariantWith<Break>(testing::Field(&Break::code, 4)), Id<Broken>()));
    done(4);

    EXPECT_EQ(4, std::get<Broken>(fsm->CurrentState()).code);
}

TEST_F(TableStateMachineTest, completion_with_is_discarded_after_transition)
{
    Build(infra::MakeRange(baseline));
    Start<Off>();
    auto done = fsm->CompletionWith<void(int)>([](int code)
        {
            return Break{ code };
        });

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(*observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm->Dispatch(Press{});

    EXPECT_CALL(*observer, EventDiscarded(Id<On>(), testing::VariantWith<Break>(testing::Field(&Break::code, 4))));
    done(4);

    EXPECT_TRUE(fsm->Is<On>());
}
