#ifndef SERVICES_STATE_MACHINE_VALIDATION_HPP
#define SERVICES_STATE_MACHINE_VALIDATION_HPP

#include "infra/stream/OutputStream.hpp"
#include "services/fsm/TransitionTableAnalysis.hpp"
#include <cstdint>
#include <optional>

namespace services
{
    template<class Machine>
    std::optional<Severity> WriteValidationReport(infra::TextOutputStream& stream, const TransitionTableAnalysis<Machine>& analysis, typename Machine::StateId initial,
        const typename TransitionTableAnalysis<Machine>::TerminalStates& terminal = {}, Severity minimum = Severity::info);

    ////    Implementation    ////

    namespace detail
    {
        template<class Machine>
        void WriteValidationRow(infra::TextOutputStream& stream, const TransitionTableAnalysis<Machine>& analysis, std::size_t index)
        {
            const auto& row = analysis.Row(index);

            stream << "row " << static_cast<uint32_t>(index) << " (" << (row.from ? Machine::StateId::FromIndex(*row.from).Name() : "*")
                   << " --" << Machine::EventId::FromIndex(row.event).Name() << "--> " << Machine::StateId::FromIndex(row.to).Name();

            if (row.guarded)
                stream << " [guarded]";
            if (row.internal)
                stream << " (internal)";

            stream << ")";
        }

        template<class Machine>
        void WriteRepeatedRowFinding(infra::TextOutputStream& stream, const TransitionTableAnalysis<Machine>& analysis, const typename TransitionTableAnalysis<Machine>::Finding& finding)
        {
            WriteValidationRow(stream, analysis, *finding.row);
            stream << (finding.kind == FindingKind::duplicateTransition ? " duplicates unguarded " : " follows unguarded ");
            WriteValidationRow(stream, analysis, *finding.otherRow);

            if (finding.kind == FindingKind::shadowedTransition)
                stream << " and is never selected";
        }

        template<class Machine>
        void WriteFindingMessage(infra::TextOutputStream& stream, const TransitionTableAnalysis<Machine>& analysis, const typename TransitionTableAnalysis<Machine>::Finding& finding, typename Machine::StateId initial)
        {
            switch (finding.kind)
            {
                case FindingKind::emptyTable:
                    stream << "the table has no rows";
                    break;
                case FindingKind::duplicateTransition:
                case FindingKind::shadowedTransition:
                    WriteRepeatedRowFinding(stream, analysis, finding);
                    break;
                case FindingKind::unreachableState:
                    stream << finding.state->Name() << " is not reachable from " << initial.Name();
                    break;
                case FindingKind::unusedEvent:
                    stream << finding.event->Name() << " is handled by no row";
                    break;
                case FindingKind::deadEndState:
                    stream << finding.state->Name() << " has no transition to another state";
                    break;
                case FindingKind::overriddenAnyRow:
                    WriteValidationRow(stream, analysis, *finding.row);
                    stream << " is overridden by an unguarded row in every state";
                    break;
                case FindingKind::canReject:
                    stream << finding.event->Name() << " in " << finding.state->Name() << " is rejected when every guard refuses";
                    break;
            }
        }
    }

    template<class Machine>
    std::optional<Severity> WriteValidationReport(infra::TextOutputStream& stream, const TransitionTableAnalysis<Machine>& analysis, typename Machine::StateId initial,
        const typename TransitionTableAnalysis<Machine>::TerminalStates& terminal, Severity minimum)
    {
        std::optional<Severity> highest;

        analysis.ForEachFinding(initial, terminal, minimum, [&](const typename TransitionTableAnalysis<Machine>::Finding& finding)
            {
                auto severity = SeverityOf(finding.kind);

                if (!highest || severity > *highest)
                    highest = severity;

                stream << NameOf(severity) << " " << NameOf(finding.kind) << ": ";
                detail::WriteFindingMessage(stream, analysis, finding, initial);
                stream << "\n";
            });

        return highest;
    }
}

#endif
