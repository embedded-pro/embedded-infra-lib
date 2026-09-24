#ifndef SERVICES_STATE_MACHINE_DEFINITION_HPP
#define SERVICES_STATE_MACHINE_DEFINITION_HPP

#include "infra/util/MemoryRange.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "services/fsm/TransitionTableAnalysis.hpp"
#include "services/fsm/ValidatedTable.hpp"
#include <array>
#include <cstddef>
#include <limits>

namespace services
{
    template<class D>
    concept StateMachineDefinition = requires {
        typename D::Machine;
        typename D::Initial;
        D::Rows();
        D::Rules();
    };

    inline constexpr std::size_t noFinding = std::numeric_limits<std::size_t>::max();

    template<class Definition>
    constexpr std::array<std::size_t, findingKindCount> FirstFindings();

    template<class Definition>
    ValidatedTable<typename Definition::Machine> Validated();

    ////    Implementation    ////

    namespace detail
    {
        template<class Finding>
        constexpr std::size_t Locate(const Finding& finding)
        {
            if (finding.row)
                return *finding.row;
            if (finding.state)
                return finding.state->Index();
            if (finding.target)
                return finding.target->Index();
            if (finding.event)
                return finding.event->Index();

            return 0;
        }

        constexpr std::size_t At(FindingKind kind)
        {
            return static_cast<std::size_t>(kind);
        }
    }

    template<class Definition>
    constexpr std::array<std::size_t, findingKindCount> FirstFindings()
    {
        using Analysis = TransitionTableAnalysis<typename Definition::Machine>;
        constexpr auto rows = Definition::Rows();

        std::array<std::size_t, findingKindCount> firsts{};
        firsts.fill(noFinding);

        Analysis(rows).ForEachFinding(Analysis::StateId::template Of<typename Definition::Initial>(), Definition::Rules(), Severity::warning, [&firsts](const typename Analysis::Finding& finding)
            {
                auto& first = firsts[detail::At(finding.kind)];
                if (first == noFinding)
                    first = detail::Locate(finding);
            });

        return firsts;
    }

    template<class Definition>
    ValidatedTable<typename Definition::Machine> Validated()
    {
        static_assert(StateMachineDefinition<Definition>, "A state machine definition provides Machine, Initial, a constexpr Rows() and a constexpr Rules()");

        using Machine = typename Definition::Machine;
        static constexpr auto rows = Definition::Rows();
        static constexpr auto firsts = FirstFindings<Definition>();

        static_assert(firsts[detail::At(FindingKind::emptyTable)] == noFinding, "emptyTable: the transition table has no rows");
        static_assert(firsts[detail::At(FindingKind::duplicateTransition)] == noFinding, "duplicateTransition: two unguarded rows for the same state and event; the value is the index of the later row");
        static_assert(firsts[detail::At(FindingKind::shadowedTransition)] == noFinding, "shadowedTransition: a row follows an unguarded row for the same state and event; the value is its index");
        static_assert(firsts[detail::At(FindingKind::unreachableState)] == noFinding, "unreachableState: a state is not reachable from Initial; the value is its index in the State variant");
        static_assert(firsts[detail::At(FindingKind::contradictoryRule)] == noFinding, "contradictoryRule: Rules() both allows and forbids a transition; the value is the index of its source state");
        static_assert(firsts[detail::At(FindingKind::forbiddenTransition)] == noFinding, "forbiddenTransition: a row makes a transition that Rules() forbids; the value is the index of the row");
        static_assert(firsts[detail::At(FindingKind::disallowedTransition)] == noFinding, "disallowedTransition: a row makes a transition that Rules() does not allow; the value is the index of the row");
        static_assert(firsts[detail::At(FindingKind::unusedEvent)] == noFinding, "unusedEvent: no row handles an event; the value is its index in the Event variant");
        static_assert(firsts[detail::At(FindingKind::deadEndState)] == noFinding, "deadEndState: no row leaves a state that is not declared Terminal; the value is its index in the State variant");
        static_assert(firsts[detail::At(FindingKind::overriddenAnyRow)] == noFinding, "overriddenAnyRow: a RowFromAny never fires because every state has an unguarded specific row; the value is its index");
        static_assert(firsts[detail::At(FindingKind::unusedAllowance)] == noFinding, "unusedAllowance: Rules() allows a transition that no row makes; the value is the index of its source state, or of its target for AllowFromAny");

        return ValidatedTable<Machine>(infra::MakeRange(rows), Machine::StateId::template Of<typename Definition::Initial>());
    }
}

#endif
