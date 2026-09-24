#ifndef SERVICES_STATE_MACHINE_MERMAID_HPP
#define SERVICES_STATE_MACHINE_MERMAID_HPP

#include "infra/stream/OutputStream.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "services/fsm/TransitionTableAnalysis.hpp"

namespace services
{
    template<class Machine>
    void WriteMermaid(infra::TextOutputStream& stream, const TransitionTableAnalysis<Machine>& analysis, typename Machine::StateId initial);

    template<class State, class Event, class Context>
    void WriteMermaid(infra::TextOutputStream& stream, const TableStateMachine<State, Event, Context>& machine, AlternativeId<State> initial);

    ////    Implementation    ////

    namespace detail
    {
        template<class Machine>
        void WriteMermaidEdge(infra::TextOutputStream& stream, typename Machine::StateId from, const typename Machine::Transition& row)
        {
            stream << "    " << from.Name() << " --> " << Machine::StateId::FromIndex(row.to).Name() << " : " << Machine::EventId::FromIndex(row.event).Name();

            if (row.guarded)
                stream << " [guarded]";
            if (row.internal)
                stream << " (internal)";

            stream << "\n";
        }

        template<class Machine>
        void WriteMermaidRow(infra::TextOutputStream& stream, const TransitionTableAnalysis<Machine>& analysis, const typename Machine::Transition& row)
        {
            using StateId = typename Machine::StateId;
            using EventId = typename Machine::EventId;

            if (row.from)
                WriteMermaidEdge<Machine>(stream, StateId::FromIndex(*row.from), row);
            else
                for (std::size_t from = 0; from != StateId::count; ++from)
                    if (!analysis.HasUnguardedSpecificRow(StateId::FromIndex(from), EventId::FromIndex(row.event)))
                        WriteMermaidEdge<Machine>(stream, StateId::FromIndex(from), row);
        }
    }

    template<class Machine>
    void WriteMermaid(infra::TextOutputStream& stream, const TransitionTableAnalysis<Machine>& analysis, typename Machine::StateId initial)
    {
        stream << "stateDiagram-v2\n";
        stream << "    [*] --> " << initial.Name() << "\n";

        for (std::size_t index = 0; index != analysis.Size(); ++index)
            detail::WriteMermaidRow(stream, analysis, analysis.Row(index));
    }

    template<class State, class Event, class Context>
    void WriteMermaid(infra::TextOutputStream& stream, const TableStateMachine<State, Event, Context>& machine, AlternativeId<State> initial)
    {
        using Machine = TableStateMachine<State, Event, Context>;
        WriteMermaid(stream, TransitionTableAnalysis<Machine>(machine.Transitions()), initial);
    }
}

#endif
