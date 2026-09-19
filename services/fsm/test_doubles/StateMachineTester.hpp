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

        template<class Machine, class F>
        static void ExpectForbiddenMatrixMatchesTable(F createMachineIn, infra::MemoryRange<const Event> sampleEvents);
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
            ASSERT_EQ(DispatchResult::transitioned, machine.Dispatch(event)) << "path towards " << target.Name() << " is blocked by " << EventId::Of(event).Name();

        ASSERT_EQ(target, machine.CurrentStateId()) << "path does not end in " << target.Name();
    }

    template<class State, class Event>
    template<class Machine, class F>
    void StateMachineTester<State, Event>::ExpectForbiddenMatrixMatchesTable(F createMachineIn, infra::MemoryRange<const Event> sampleEvents)
    {
        ForEachStateAndEvent(sampleEvents, [&](StateId state, const Event& event)
            {
                Machine machine;
                createMachineIn(machine, state);
                ASSERT_EQ(state, machine.CurrentStateId());

                bool allowed = machine.HasTransition(state, EventId::Of(event));
                auto result = machine.Dispatch(event);
                EXPECT_EQ(allowed, result != DispatchResult::forbidden) << EventId::Of(event).Name() << " in " << state.Name();
            });
    }
}

#endif
