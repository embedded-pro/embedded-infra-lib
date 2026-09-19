#ifndef SERVICES_STATE_TIMEOUTS_HPP
#define SERVICES_STATE_TIMEOUTS_HPP

#include "infra/timer/Timer.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/fsm/StateMachine.hpp"

namespace services
{
    template<class State, class Event>
    struct StateTimeout
    {
        AlternativeId<State> state;
        infra::Duration duration;
        Event event;
    };

    template<class State, class Event>
    class StateTimeouts
        : public StateMachineObserver<State, Event>
    {
    public:
        using StateId = AlternativeId<State>;
        using Timeout = StateTimeout<State, Event>;

        StateTimeouts(StateMachine<State, Event>& subject, infra::MemoryRange<const Timeout> timeouts);

        void Started(StateId initial) override;
        void StateChanged(StateId from, const Event& event, StateId to) override;

    private:
        void Arm(StateId state);
        const Timeout* TimeoutFor(StateId state) const;

    private:
        infra::MemoryRange<const Timeout> timeouts;
        infra::TimerSingleShot timer;
    };

    ////    Implementation    ////

    template<class State, class Event>
    StateTimeouts<State, Event>::StateTimeouts(StateMachine<State, Event>& subject, infra::MemoryRange<const Timeout> timeouts)
        : StateMachineObserver<State, Event>(subject)
        , timeouts(timeouts)
    {}

    template<class State, class Event>
    void StateTimeouts<State, Event>::Started(StateId initial)
    {
        Arm(initial);
    }

    template<class State, class Event>
    void StateTimeouts<State, Event>::StateChanged(StateId from, const Event& event, StateId to)
    {
        Arm(to);
    }

    template<class State, class Event>
    void StateTimeouts<State, Event>::Arm(StateId state)
    {
        timer.Cancel();

        const Timeout* timeout = TimeoutFor(state);
        if (timeout != nullptr)
            timer.Start(timeout->duration, [this, timeout]()
                {
                    this->Subject().Dispatch(timeout->event);
                });
    }

    template<class State, class Event>
    const typename StateTimeouts<State, Event>::Timeout* StateTimeouts<State, Event>::TimeoutFor(StateId state) const
    {
        for (const auto& timeout : timeouts)
            if (timeout.state == state)
                return &timeout;

        return nullptr;
    }
}

#endif
