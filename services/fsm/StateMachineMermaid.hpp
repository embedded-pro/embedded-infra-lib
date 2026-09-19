#ifndef SERVICES_STATE_MACHINE_MERMAID_HPP
#define SERVICES_STATE_MACHINE_MERMAID_HPP

#include "infra/stream/OutputStream.hpp"
#include "services/fsm/TableStateMachine.hpp"

namespace services
{
    template<class State, class Event>
    void WriteMermaid(infra::TextOutputStream& stream, const TableStateMachine<State, Event>& machine, AlternativeId<State> initial);

    ////    Implementation    ////

    namespace detail
    {
        template<class State, class Event>
        void WriteMermaidRow(infra::TextOutputStream& stream, const typename TableStateMachine<State, Event>::Transition& row)
        {
            const char* from = row.from ? AlternativeId<State>::FromIndex(*row.from).Name() : "[*]";
            stream << "    " << from << " --> " << AlternativeId<State>::FromIndex(row.to).Name() << " : " << AlternativeId<Event>::FromIndex(row.event).Name();

            if (row.guarded)
                stream << " [guarded]";
            if (row.internal)
                stream << " (internal)";
            if (!row.from)
                stream << " (from any state)";

            stream << "\n";
        }
    }

    template<class State, class Event>
    void WriteMermaid(infra::TextOutputStream& stream, const TableStateMachine<State, Event>& machine, AlternativeId<State> initial)
    {
        stream << "stateDiagram-v2\n";
        stream << "    [*] --> " << initial.Name() << "\n";

        for (const auto& row : machine.Transitions())
            detail::WriteMermaidRow<State, Event>(stream, row);
    }
}

#endif
