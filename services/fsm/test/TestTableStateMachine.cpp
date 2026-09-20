#include "services/fsm/TableStateMachine.hpp"
#include "services/fsm/test_doubles/StateMachineObserverMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

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

    using StateId = services::AlternativeId<State>;
    using Machine = services::TableStateMachine<State, Event>;

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
}

class TableStateMachineTest
    : public testing::Test
{
public:
    void AddCommon()
    {
        fsm.Add<On, Press, Off>()
            .AddFromAny<Break, Broken>(nullptr, [](State&, const Break& event)
                {
                    return Broken{ event.code };
                })
            .Add<Broken, Fix, Off>()
            .Add<Off, Lock, Locked>(nullptr, [this](Off&, const Lock&)
                {
                    return Locked{ fsm, hooks };
                })
            .Add<Locked, Fix, Off>();
    }

    void AddBaseline()
    {
        AddCommon();
        fsm.Add<Off, Press, On>(nullptr, [this](Off&, const Press&)
            {
                return On{ &hooks };
            });
    }

    template<class Initial, class... Args>
    void Start(Args&&... args)
    {
        if constexpr (std::is_same_v<Initial, On>)
            EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
        if constexpr (std::is_same_v<Initial, Locked>)
            EXPECT_CALL(hooks, Entry(testing::StrEq("Locked committed")));
        EXPECT_CALL(observer, Started(Id<Initial>()));
        fsm.Start<Initial>(std::forward<Args>(args)...);
    }

    testing::StrictMock<Hooks> hooks;
    Machine::WithStorage<16, 4> fsm;
    testing::StrictMock<services::StateMachineObserverMock<State, Event>> observer{ fsm };
};

TEST_F(TableStateMachineTest, valid_table_has_no_consistency_error)
{
    AddBaseline();

    EXPECT_EQ(services::ConsistencyError::none, fsm.CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, empty_table_is_rejected)
{
    EXPECT_EQ(services::ConsistencyError::emptyTable, fsm.CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, two_unguarded_rows_for_same_pair_are_duplicate)
{
    AddBaseline();
    fsm.Add<Off, Press, Broken>();

    EXPECT_EQ(services::ConsistencyError::duplicateTransition, fsm.CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, row_after_unguarded_row_is_shadowed)
{
    AddBaseline();
    fsm.Add<Off, Press, Broken>([](const Off&, const Press&)
        {
            return true;
        });

    EXPECT_EQ(services::ConsistencyError::shadowedTransition, fsm.CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, guarded_rows_for_same_pair_are_allowed)
{
    AddCommon();
    fsm.Add<Off, Press, On>([](const Off&, const Press&)
           {
               return true;
           },
           [this](Off&, const Press&)
           {
               return On{ &hooks };
           })
        .Add<Off, Press, Broken>();

    EXPECT_EQ(services::ConsistencyError::none, fsm.CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, state_class_not_reachable_from_initial_is_rejected)
{
    fsm.Add<Off, Press, On>(nullptr, [this](Off&, const Press&)
        {
            return On{ &hooks };
        });

    EXPECT_EQ(services::ConsistencyError::unreachableState, fsm.CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, any_state_row_makes_target_reachable)
{
    fsm.Add<Off, Press, On>(nullptr, [this](Off&, const Press&)
           {
               return On{ &hooks };
           })
        .Add<Off, Lock, Locked>(nullptr, [this](Off&, const Lock&)
            {
                return Locked{ fsm, hooks };
            });
    EXPECT_EQ(services::ConsistencyError::unreachableState, fsm.CheckConsistency<Off>());

    fsm.AddFromAny<Break, Broken>();
    EXPECT_EQ(services::ConsistencyError::none, fsm.CheckConsistency<Off>());
}

TEST_F(TableStateMachineTest, has_transition_reports_specific_and_wildcard_rows)
{
    AddBaseline();

    EXPECT_TRUE((fsm.HasTransition<Off, Press>()));
    EXPECT_TRUE((fsm.HasTransition<On, Break>()));
    EXPECT_FALSE((fsm.HasTransition<On, Fix>()));
    EXPECT_TRUE(fsm.HasTransition(Id<Locked>(), services::AlternativeId<Event>::Of<Fix>()));
}

TEST_F(TableStateMachineTest, transitions_exposes_rows_in_insertion_order)
{
    AddBaseline();

    auto rows = fsm.Transitions();
    ASSERT_EQ(6, rows.size());
    EXPECT_EQ(Id<On>().Index(), *rows[0].from);
    EXPECT_FALSE(rows[1].from.has_value());
    EXPECT_EQ(Id<Broken>().Index(), rows[1].to);
}

TEST_F(TableStateMachineTest, add_after_start_asserts)
{
    AddBaseline();
    Start<Off>();

    EXPECT_DEATH((fsm.Add<Off, Fix, Off>()), "");
}

TEST_F(TableStateMachineTest, start_with_inconsistent_table_asserts)
{
    EXPECT_DEATH(fsm.Start<Off>(), "");
}

TEST_F(TableStateMachineTest, start_constructs_initial_state_runs_its_entry_and_notifies_started)
{
    AddBaseline();

    Start<On>(On{ &hooks, 4 });

    EXPECT_TRUE(fsm.Started());
    EXPECT_TRUE(fsm.Is<On>());
    EXPECT_EQ(4, std::get<On>(fsm.CurrentState()).level);
    EXPECT_EQ(Id<On>(), fsm.CurrentStateId());
}

TEST_F(TableStateMachineTest, dispatch_before_start_asserts)
{
    AddBaseline();

    EXPECT_FALSE(fsm.Started());
    EXPECT_DEATH(fsm.Dispatch(Press{}), "");
}

TEST_F(TableStateMachineTest, matching_row_runs_exit_action_entry_then_notifies_in_order)
{
    AddCommon();
    fsm.Add<Off, Press, On>(nullptr, [this](Off&, const Press&)
        {
            hooks.Action("build On");
            return On{ &hooks };
        });
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Action(testing::StrEq("build On")));
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_EQ(services::DispatchResult::transitioned, fsm.Dispatch(Press{}));

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), With<Press>(), Id<Off>()));
    EXPECT_EQ(services::DispatchResult::transitioned, fsm.Dispatch(Press{}));
    EXPECT_TRUE(fsm.Is<Off>());
}

TEST_F(TableStateMachineTest, action_receives_typed_source_state_and_event_and_builds_target)
{
    AddCommon();
    fsm.Add<Off, Press, On>(nullptr, [this](Off&, const Press&)
        {
            return On{ &hooks, 3 };
        });
    Start<Off>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm.Dispatch(Press{});
    EXPECT_EQ(3, std::get<On>(fsm.CurrentState()).level);

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), testing::VariantWith<Break>(testing::Field(&Break::code, 7)), Id<Broken>()));
    fsm.Dispatch(Break{ 7 });
    EXPECT_EQ(7, std::get<Broken>(fsm.CurrentState()).code);
}

TEST_F(TableStateMachineTest, omitted_action_default_constructs_target)
{
    AddBaseline();
    Start<Broken>(Broken{ 2 });

    EXPECT_CALL(observer, StateChanged(Id<Broken>(), With<Fix>(), Id<Off>()));
    fsm.Dispatch(Fix{});

    EXPECT_TRUE(fsm.Is<Off>());
}

TEST_F(TableStateMachineTest, entry_runs_on_committed_state)
{
    AddBaseline();
    Start<Off>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("Locked committed")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Lock>(), Id<Locked>()));
    fsm.Dispatch(Lock{});
}

TEST_F(TableStateMachineTest, event_without_row_is_forbidden_and_notifies_without_changing_state)
{
    AddBaseline();
    Start<Off>();

    EXPECT_CALL(observer, EventForbidden(Id<Off>(), With<Fix>()));
    EXPECT_EQ(services::DispatchResult::forbidden, fsm.Dispatch(Fix{}));

    EXPECT_TRUE(fsm.Is<Off>());
}

TEST_F(TableStateMachineTest, event_whose_guards_all_refuse_is_rejected_and_notifies)
{
    AddCommon();
    fsm.Add<Off, Press, On>([this](const Off&, const Press&)
        {
            return hooks.Guard("allow");
        },
        [this](Off&, const Press&)
        {
            return On{ &hooks };
        });
    Start<Off>();

    EXPECT_CALL(hooks, Guard(testing::StrEq("allow"))).WillOnce(testing::Return(false));
    EXPECT_CALL(observer, EventRejected(Id<Off>(), With<Press>()));
    EXPECT_EQ(services::DispatchResult::rejected, fsm.Dispatch(Press{}));

    EXPECT_TRUE(fsm.Is<Off>());
}

TEST_F(TableStateMachineTest, first_row_whose_guard_accepts_wins_and_later_guards_are_not_evaluated)
{
    AddCommon();
    fsm.Add<Off, Press, On>([this](const Off&, const Press&)
           {
               return hooks.Guard("first");
           },
           [this](Off&, const Press&)
           {
               return On{ &hooks };
           })
        .Add<Off, Press, Broken>([this](const Off&, const Press&)
            {
                return hooks.Guard("second");
            });
    Start<Off>();

    EXPECT_CALL(hooks, Guard(testing::StrEq("first"))).WillOnce(testing::Return(false));
    EXPECT_CALL(hooks, Guard(testing::StrEq("second"))).WillOnce(testing::Return(true));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<Broken>()));
    fsm.Dispatch(Press{});

    EXPECT_CALL(observer, StateChanged(Id<Broken>(), With<Fix>(), Id<Off>()));
    fsm.Dispatch(Fix{});

    EXPECT_CALL(hooks, Guard(testing::StrEq("first"))).WillOnce(testing::Return(true));
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm.Dispatch(Press{});
}

TEST_F(TableStateMachineTest, specific_row_beats_any_state_row)
{
    AddBaseline();
    fsm.Add<On, Break, Off>();
    Start<On>(On{ &hooks });

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), With<Break>(), Id<Off>()));
    fsm.Dispatch(Break{});
    EXPECT_TRUE(fsm.Is<Off>());

    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Break>(), Id<Broken>()));
    fsm.Dispatch(Break{});
    EXPECT_TRUE(fsm.Is<Broken>());
}

TEST_F(TableStateMachineTest, any_state_row_is_fallback_when_specific_guard_refuses)
{
    AddBaseline();
    fsm.Add<On, Break, Off>([this](const On&, const Break&)
        {
            return hooks.Guard("specific");
        });
    Start<On>(On{ &hooks });

    EXPECT_CALL(hooks, Guard(testing::StrEq("specific"))).WillOnce(testing::Return(false));
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), With<Break>(), Id<Broken>()));
    fsm.Dispatch(Break{});
}

TEST_F(TableStateMachineTest, any_state_row_fires_from_every_state_with_event_payload)
{
    AddBaseline();
    Start<Off>();

    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Lock>(), Id<Locked>()));
    EXPECT_CALL(hooks, Entry(testing::StrEq("Locked committed")));
    fsm.Dispatch(Lock{});

    EXPECT_CALL(observer, StateChanged(Id<Locked>(), With<Break>(), Id<Broken>()));
    fsm.Dispatch(Break{ 5 });
    EXPECT_EQ(5, std::get<Broken>(fsm.CurrentState()).code);

    EXPECT_CALL(observer, StateChanged(Id<Broken>(), With<Break>(), Id<Broken>()));
    fsm.Dispatch(Break{ 6 });
    EXPECT_EQ(6, std::get<Broken>(fsm.CurrentState()).code);
}

TEST_F(TableStateMachineTest, internal_row_runs_action_without_exit_or_entry)
{
    AddBaseline();
    fsm.AddInternal<On, Dim>([this](On& state, const Dim& event)
        {
            hooks.Action("dim");
            state.level += event.amount;
        });
    Start<On>(On{ &hooks, 10 });

    EXPECT_CALL(hooks, Action(testing::StrEq("dim")));
    EXPECT_CALL(observer, EventHandled(Id<On>(), With<Dim>()));
    EXPECT_EQ(services::DispatchResult::transitioned, fsm.Dispatch(Dim{ -3 }));

    EXPECT_EQ(7, std::get<On>(fsm.CurrentState()).level);
}

TEST_F(TableStateMachineTest, internal_row_without_action_ignores_event_and_notifies_event_handled)
{
    AddBaseline();
    fsm.AddInternal<Off, Dim>();
    Start<Off>();

    EXPECT_CALL(observer, EventHandled(Id<Off>(), With<Dim>()));
    EXPECT_EQ(services::DispatchResult::transitioned, fsm.Dispatch(Dim{ 1 }));

    EXPECT_TRUE(fsm.Is<Off>());
}

TEST_F(TableStateMachineTest, guarded_internal_row_is_rejected_when_guard_refuses)
{
    AddBaseline();
    fsm.AddInternal<Off, Dim>(nullptr, [this](const Off&, const Dim&)
        {
            return hooks.Guard("dim");
        });
    Start<Off>();

    EXPECT_CALL(hooks, Guard(testing::StrEq("dim"))).WillOnce(testing::Return(false));
    EXPECT_CALL(observer, EventRejected(Id<Off>(), With<Dim>()));
    EXPECT_EQ(services::DispatchResult::rejected, fsm.Dispatch(Dim{ 1 }));
}

TEST_F(TableStateMachineTest, external_self_transition_runs_exit_and_entry)
{
    AddBaseline();
    fsm.Add<On, Lock, On>(nullptr, [this](On& from, const Lock&)
        {
            return On{ &hooks, from.level + 1 };
        });
    Start<On>(On{ &hooks, 1 });

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), With<Lock>(), Id<On>()));
    fsm.Dispatch(Lock{});

    EXPECT_EQ(2, std::get<On>(fsm.CurrentState()).level);
}

TEST_F(TableStateMachineTest, state_with_reference_member_can_self_transition)
{
    AddBaseline();
    fsm.Add<Locked, Lock, Locked>(nullptr, [this](Locked&, const Lock&)
        {
            return Locked{ fsm, hooks };
        });
    Start<Locked>(Locked{ fsm, hooks });

    EXPECT_CALL(hooks, Entry(testing::StrEq("Locked committed")));
    EXPECT_CALL(observer, StateChanged(Id<Locked>(), With<Lock>(), Id<Locked>()));
    fsm.Dispatch(Lock{});
}

TEST_F(TableStateMachineTest, dispatch_from_action_is_queued_and_handled_after_commit)
{
    AddCommon();
    std::optional<services::DispatchResult> nestedResult;
    fsm.Add<Off, Press, On>(nullptr, [this, &nestedResult](Off&, const Press&)
        {
            nestedResult = fsm.Dispatch(Press{});
            return On{ &hooks };
        });
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), With<Press>(), Id<Off>()));
    EXPECT_EQ(services::DispatchResult::transitioned, fsm.Dispatch(Press{}));

    EXPECT_EQ(services::DispatchResult::queued, nestedResult);
    EXPECT_TRUE(fsm.Is<Off>());
}

TEST_F(TableStateMachineTest, dispatch_from_observer_is_queued)
{
    AddBaseline();
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>())).WillOnce(testing::Invoke([this]()
        {
            EXPECT_EQ(services::DispatchResult::queued, fsm.Dispatch(Break{ 1 }));
        }));
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), With<Break>(), Id<Broken>()));
    fsm.Dispatch(Press{});

    EXPECT_TRUE(fsm.Is<Broken>());
}

TEST_F(TableStateMachineTest, queued_events_are_handled_in_order)
{
    AddCommon();
    fsm.Add<Off, Press, On>(nullptr, [this](Off&, const Press&)
        {
            fsm.Dispatch(Break{ 1 });
            fsm.Dispatch(Fix{});
            return On{ &hooks };
        });
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), With<Break>(), Id<Broken>()));
    EXPECT_CALL(observer, StateChanged(Id<Broken>(), With<Fix>(), Id<Off>()));
    fsm.Dispatch(Press{});

    EXPECT_TRUE(fsm.Is<Off>());
}

TEST_F(TableStateMachineTest, dispatch_during_start_is_queued_until_initial_state_is_committed)
{
    AddBaseline();

    EXPECT_CALL(observer, Started(Id<Off>())).WillOnce(testing::Invoke([this]()
        {
            EXPECT_EQ(services::DispatchResult::queued, fsm.Dispatch(Press{}));
        }));
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm.Start<Off>();

    EXPECT_TRUE(fsm.Is<On>());
}

namespace
{
    void OverflowQueue()
    {
        Machine::WithStorage<4, 1> fsm;
        fsm.Add<Off, Press, Off>(nullptr, [&fsm](Off&, const Press&)
               {
                   fsm.Dispatch(Press{});
                   fsm.Dispatch(Press{});
                   return Off{};
               })
            .Add<Off, Fix, On>()
            .AddFromAny<Break, Broken>()
            .Add<Off, Lock, Locked>(nullptr, [](Off&, const Lock&) -> Locked
                {
                    std::abort();
                });
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
    AddBaseline();
    Start<Off>();

    auto done = fsm.Completion<Press>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    done();

    EXPECT_TRUE(fsm.Is<On>());
}

TEST_F(TableStateMachineTest, completion_after_transition_is_discarded_and_notifies)
{
    AddBaseline();
    Start<Off>();
    auto done = fsm.Completion<Press>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm.Dispatch(Press{});

    EXPECT_CALL(observer, EventDiscarded(Id<On>(), With<Press>()));
    done();

    EXPECT_TRUE(fsm.Is<On>());
}

TEST_F(TableStateMachineTest, completion_after_self_transition_is_discarded)
{
    AddBaseline();
    fsm.Add<On, Lock, On>(nullptr, [this](On&, const Lock&)
        {
            return On{ &hooks };
        });
    Start<On>(On{ &hooks });
    auto done = fsm.Completion<Press>();

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), With<Lock>(), Id<On>()));
    fsm.Dispatch(Lock{});

    EXPECT_CALL(observer, EventDiscarded(Id<On>(), With<Press>()));
    done();
}

TEST_F(TableStateMachineTest, completion_created_in_action_belongs_to_target_state)
{
    AddCommon();
    infra::Function<void()> done;
    fsm.Add<Off, Press, On>(nullptr, [this, &done](Off&, const Press&)
        {
            done = fsm.Completion<Press>();
            return On{ &hooks };
        });
    Start<Off>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm.Dispatch(Press{});

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), With<Press>(), Id<Off>()));
    done();

    EXPECT_TRUE(fsm.Is<Off>());
}

TEST_F(TableStateMachineTest, completion_is_not_discarded_by_internal_transition)
{
    AddBaseline();
    fsm.AddInternal<Off, Dim>();
    Start<Off>();
    auto done = fsm.Completion<Press>();

    EXPECT_CALL(observer, EventHandled(Id<Off>(), With<Dim>()));
    fsm.Dispatch(Dim{});

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    done();
}

TEST_F(TableStateMachineTest, completion_with_payload_carries_payload)
{
    AddBaseline();
    Start<Off>();

    auto done = fsm.Completion<Break, 4 * sizeof(void*)>(Break{ 9 });

    EXPECT_CALL(observer, StateChanged(Id<Off>(), testing::VariantWith<Break>(testing::Field(&Break::code, 9)), Id<Broken>()));
    done();

    EXPECT_EQ(9, std::get<Broken>(fsm.CurrentState()).code);
}

TEST_F(TableStateMachineTest, observer_may_detach_during_notification)
{
    AddBaseline();
    testing::StrictMock<services::StateMachineObserverMock<State, Event>> second{ fsm };
    EXPECT_CALL(second, Started(Id<Off>()));
    Start<Off>();

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_CALL(second, StateChanged(Id<Off>(), With<Press>(), Id<On>())).WillOnce(testing::Invoke([&second]()
        {
            second.Detach();
        }));
    fsm.Dispatch(Press{});

    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), With<Press>(), Id<Off>()));
    fsm.Dispatch(Press{});
}

TEST_F(TableStateMachineTest, current_state_is_readable_while_dispatching)
{
    AddCommon();
    fsm.Add<Off, Press, On>(nullptr, [this](Off&, const Press&)
        {
            EXPECT_TRUE(fsm.Dispatching());
            EXPECT_TRUE(fsm.Is<Off>());
            return On{ &hooks };
        });
    Start<Off>();
    EXPECT_FALSE(fsm.Dispatching());

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm.Dispatch(Press{});

    EXPECT_FALSE(fsm.Dispatching());
}

TEST_F(TableStateMachineTest, on_entered_hook_runs_after_observers_when_state_is_entered)
{
    AddBaseline();
    fsm.OnEntered<On>([this]()
        {
            hooks.Action("entered On");
        });
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_CALL(hooks, Action(testing::StrEq("entered On")));
    fsm.Dispatch(Press{});
}

TEST_F(TableStateMachineTest, on_entered_hook_runs_for_initial_state_on_start)
{
    AddBaseline();
    fsm.OnEntered<Off>([this]()
        {
            hooks.Action("entered Off");
        });

    testing::InSequence sequence;
    EXPECT_CALL(observer, Started(Id<Off>()));
    EXPECT_CALL(hooks, Action(testing::StrEq("entered Off")));
    fsm.Start<Off>();
}

TEST_F(TableStateMachineTest, internal_row_does_not_run_on_entered_hook)
{
    AddBaseline();
    fsm.AddInternal<Off, Dim>();
    fsm.OnEntered<Off>([this]()
        {
            hooks.Action("entered Off");
        });
    EXPECT_CALL(hooks, Action(testing::StrEq("entered Off")));
    Start<Off>();

    EXPECT_CALL(observer, EventHandled(Id<Off>(), With<Dim>()));
    fsm.Dispatch(Dim{});
}

TEST_F(TableStateMachineTest, dispatch_from_on_entered_hook_is_queued_and_handled_afterwards)
{
    AddBaseline();
    fsm.OnEntered<On>([this]()
        {
            EXPECT_EQ(services::DispatchResult::queued, fsm.Dispatch(Press{}));
        });
    Start<Off>();

    testing::InSequence sequence;
    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    EXPECT_CALL(hooks, Exit(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<On>(), With<Press>(), Id<Off>()));
    fsm.Dispatch(Press{});

    EXPECT_TRUE(fsm.Is<Off>());
}

TEST_F(TableStateMachineTest, on_entered_after_start_asserts)
{
    AddBaseline();
    Start<Off>();

    EXPECT_DEATH((fsm.OnEntered<Off>([]() {})), "");
}

TEST_F(TableStateMachineTest, completion_with_maps_callback_arguments_to_event)
{
    AddBaseline();
    Start<Off>();

    auto done = fsm.CompletionWith<void(int)>([](int code)
        {
            return Break{ code };
        });

    EXPECT_CALL(observer, StateChanged(Id<Off>(), testing::VariantWith<Break>(testing::Field(&Break::code, 4)), Id<Broken>()));
    done(4);

    EXPECT_EQ(4, std::get<Broken>(fsm.CurrentState()).code);
}

TEST_F(TableStateMachineTest, completion_with_is_discarded_after_transition)
{
    AddBaseline();
    Start<Off>();
    auto done = fsm.CompletionWith<void(int)>([](int code)
        {
            return Break{ code };
        });

    EXPECT_CALL(hooks, Entry(testing::StrEq("On")));
    EXPECT_CALL(observer, StateChanged(Id<Off>(), With<Press>(), Id<On>()));
    fsm.Dispatch(Press{});

    EXPECT_CALL(observer, EventDiscarded(Id<On>(), testing::VariantWith<Break>(testing::Field(&Break::code, 4))));
    done(4);

    EXPECT_TRUE(fsm.Is<On>());
}
