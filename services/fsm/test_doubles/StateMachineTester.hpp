#ifndef SERVICES_STATE_MACHINE_TESTER_HPP
#define SERVICES_STATE_MACHINE_TESTER_HPP

#include "infra/util/MemoryRange.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "gtest/gtest.h"

namespace services
{
    template<class State, class Event>
    class StateMachineTester
    {
    public:
        using StateId = AlternativeId<State>;
        using EventId = AlternativeId<Event>;

        template<class F>
        static void ForEachStateAndEvent(infra::MemoryRange<const Event> sampleEvents, F callback);

        static void DriveTo(StateMachine<State, Event>& machine, StateId target, infra::MemoryRange<const Event> path);

        template<class F>
        static void ExpectForbiddenMatrixMatchesTable(F machineIn, infra::MemoryRange<const Event> sampleEvents);
    };

    ////    Implementation    ////

    template<class State, class Event>
    template<class F>
    void StateMachineTester<State, Event>::ForEachStateAndEvent(infra::MemoryRange<const Event> sampleEvents, F callback)
    {
        for (std::size_t state = 0; state != StateId::count; ++state)
            for (const auto& event : sampleEvents)
                callback(StateId::FromIndex(state), event);
    }

    template<class State, class Event>
    void StateMachineTester<State, Event>::DriveTo(StateMachine<State, Event>& machine, StateId target, infra::MemoryRange<const Event> path)
    {
        for (const auto& event : path)
            ASSERT_EQ(DispatchResult::transitioned, machine.Dispatch(event)) << "path towards " << target.Name() << " is blocked by " << EventId::Of(event).Name() << " in " << machine.CurrentStateId().Name();

        ASSERT_EQ(target, machine.CurrentStateId()) << "path ends in " << machine.CurrentStateId().Name() << " instead of " << target.Name();
    }

    template<class State, class Event>
    template<class F>
    void StateMachineTester<State, Event>::ExpectForbiddenMatrixMatchesTable(F machineIn, infra::MemoryRange<const Event> sampleEvents)
    {
        ForEachStateAndEvent(sampleEvents, [&](StateId state, const Event& event)
            {
                TableStateMachine<State, Event>& machine = machineIn(state);
                ASSERT_EQ(state, machine.CurrentStateId());

                bool allowed = machine.HasTransition(state, EventId::Of(event));
                bool accepted = machine.Dispatch(event) != DispatchResult::forbidden;
                EXPECT_EQ(allowed, accepted) << EventId::Of(event).Name() << " in " << state.Name() << (allowed ? " is in the table but was forbidden" : " is not in the table but was accepted");
            });
    }
}

#endif
