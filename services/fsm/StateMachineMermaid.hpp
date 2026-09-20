#ifndef SERVICES_STATE_MACHINE_MERMAID_HPP
#define SERVICES_STATE_MACHINE_MERMAID_HPP

#include "infra/stream/OutputStream.hpp"
#include "services/fsm/TableStateMachine.hpp"

namespace services
{
    template<class State, class Event, class Context>
    void WriteMermaid(infra::TextOutputStream& stream, const TableStateMachine<State, Event, Context>& machine, AlternativeId<State> initial);

    ////    Implementation    ////

    namespace detail
    {
        template<class State, class Event, class Row>
        void WriteMermaidEdge(infra::TextOutputStream& stream, AlternativeId<State> from, const Row& row)
        {
            stream << "    " << from.Name() << " --> " << AlternativeId<State>::FromIndex(row.to).Name() << " : " << AlternativeId<Event>::FromIndex(row.event).Name();

            if (row.guarded)
                stream << " [guarded]";
            if (row.internal)
                stream << " (internal)";

            stream << "\n";
        }

        template<class State, class Event, class Row>
        void WriteMermaidRow(infra::TextOutputStream& stream, const Row& row)
        {
            if (row.from)
                WriteMermaidEdge<State, Event>(stream, AlternativeId<State>::FromIndex(*row.from), row);
            else
                for (std::size_t from = 0; from != AlternativeId<State>::count; ++from)
                    WriteMermaidEdge<State, Event>(stream, AlternativeId<State>::FromIndex(from), row);
        }
    }

    template<class State, class Event, class Context>
    void WriteMermaid(infra::TextOutputStream& stream, const TableStateMachine<State, Event, Context>& machine, AlternativeId<State> initial)
    {
        stream << "stateDiagram-v2\n";
        stream << "    [*] --> " << initial.Name() << "\n";

        for (const auto& row : machine.Transitions())
            detail::WriteMermaidRow<State, Event>(stream, row);
    }
}

#endif
