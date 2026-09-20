#ifndef SERVICES_STATE_MACHINE_OBSERVER_MOCK_HPP
#define SERVICES_STATE_MACHINE_OBSERVER_MOCK_HPP

#include "services/fsm/StateMachine.hpp"
#include "gmock/gmock.h"
#include <ostream>

namespace services
{
    template<class Variant>
    void PrintTo(const AlternativeId<Variant>& id, std::ostream* os)
    {
        *os << id.Name();
    }

    template<class State, class Event>
    class StateMachineObserverMock
        : public StateMachineObserver<State, Event>
    {
    public:
        using StateId = AlternativeId<State>;
        using StateMachineObserver<State, Event>::StateMachineObserver;

        MOCK_METHOD(void, Started, (StateId initial), (override));
        MOCK_METHOD(void, StateChanged, (StateId from, const Event& event, StateId to), (override));
        MOCK_METHOD(void, EventHandled, (StateId state, const Event& event), (override));
        MOCK_METHOD(void, EventForbidden, (StateId state, const Event& event), (override));
        MOCK_METHOD(void, EventRejected, (StateId state, const Event& event), (override));
        MOCK_METHOD(void, EventDiscarded, (StateId state, const Event& event), (override));
    };
}

#endif
