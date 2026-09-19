#ifndef SERVICES_STATE_MACHINE_TRACER_HPP
#define SERVICES_STATE_MACHINE_TRACER_HPP

#include "services/fsm/StateMachine.hpp"
#include "services/tracer/Tracer.hpp"

namespace services
{
    template<class State, class Event>
    class StateMachineTracer
        : public StateMachineObserver<State, Event>
    {
    public:
        using StateId = AlternativeId<State>;
        using EventId = AlternativeId<Event>;

        StateMachineTracer(StateMachine<State, Event>& subject, Tracer& tracer);

        void Started(StateId initial) override;
        void StateChanged(StateId from, const Event& event, StateId to) override;
        void EventForbidden(StateId state, const Event& event) override;
        void EventRejected(StateId state, const Event& event) override;
        void EventDiscarded(StateId state, const Event& event) override;

    private:
        void TraceEventIn(const char* verdict, StateId state, const Event& event);

    private:
        Tracer& tracer;
    };

    ////    Implementation    ////

    template<class State, class Event>
    StateMachineTracer<State, Event>::StateMachineTracer(StateMachine<State, Event>& subject, Tracer& tracer)
        : StateMachineObserver<State, Event>(subject)
        , tracer(tracer)
    {}

    template<class State, class Event>
    void StateMachineTracer<State, Event>::Started(StateId initial)
    {
        tracer.Trace() << "fsm: started in " << initial.Name();
    }

    template<class State, class Event>
    void StateMachineTracer<State, Event>::StateChanged(StateId from, const Event& event, StateId to)
    {
        tracer.Trace() << "fsm: " << from.Name() << " --" << EventId::Of(event).Name() << "--> " << to.Name();
    }

    template<class State, class Event>
    void StateMachineTracer<State, Event>::EventForbidden(StateId state, const Event& event)
    {
        TraceEventIn("forbidden", state, event);
    }

    template<class State, class Event>
    void StateMachineTracer<State, Event>::EventRejected(StateId state, const Event& event)
    {
        TraceEventIn("rejected", state, event);
    }

    template<class State, class Event>
    void StateMachineTracer<State, Event>::EventDiscarded(StateId state, const Event& event)
    {
        TraceEventIn("discarded", state, event);
    }

    template<class State, class Event>
    void StateMachineTracer<State, Event>::TraceEventIn(const char* verdict, StateId state, const Event& event)
    {
        tracer.Trace() << "fsm: " << verdict << " " << EventId::Of(event).Name() << " in " << state.Name();
    }
}

#endif
