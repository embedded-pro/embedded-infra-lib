#ifndef SERVICES_STATE_MACHINE_HPP
#define SERVICES_STATE_MACHINE_HPP

#include "infra/util/Observer.hpp"
#include "services/fsm/AlternativeId.hpp"
#include <cstdint>

namespace services
{
    enum class DispatchResult : uint8_t
    {
        transitioned,
        rejected,
        forbidden,
        queued
    };

    template<class State, class Event>
    class StateMachine;

    template<class State, class Event>
    class StateMachineObserver
        : public infra::Observer<StateMachineObserver<State, Event>, StateMachine<State, Event>>
    {
    public:
        using StateId = AlternativeId<State>;
        using infra::Observer<StateMachineObserver<State, Event>, StateMachine<State, Event>>::Observer;

        virtual void Started(StateId initial)
        {}

        virtual void StateChanged(StateId from, const Event& event, StateId to) = 0;

        virtual void EventForbidden(StateId state, const Event& event)
        {}

        virtual void EventRejected(StateId state, const Event& event)
        {}

        virtual void EventDiscarded(StateId state, const Event& event)
        {}
    };

    template<class State, class Event>
    class StateMachine
        : public infra::Subject<StateMachineObserver<State, Event>>
    {
    public:
        using StateId = AlternativeId<State>;
        using EventId = AlternativeId<Event>;

        virtual const State& CurrentState() const = 0;
        virtual DispatchResult Dispatch(const Event& event) = 0;

        StateId CurrentStateId() const;

        template<class S>
        bool Is() const;
    };

    ////    Implementation    ////

    template<class State, class Event>
    typename StateMachine<State, Event>::StateId StateMachine<State, Event>::CurrentStateId() const
    {
        return StateId::Of(CurrentState());
    }

    template<class State, class Event>
    template<class S>
    bool StateMachine<State, Event>::Is() const
    {
        return std::holds_alternative<S>(CurrentState());
    }
}

#endif
