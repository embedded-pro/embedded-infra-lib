#ifndef SERVICES_TRANSITION_TABLE_ANALYSIS_HPP
#define SERVICES_TRANSITION_TABLE_ANALYSIS_HPP

#include "services/fsm/AlternativeId.hpp"
#include "services/fsm/TransitionRules.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace services
{
    enum class ConsistencyError : uint8_t
    {
        none,
        emptyTable,
        duplicateTransition,
        shadowedTransition,
        unreachableState
    };

    enum class FindingKind : uint8_t
    {
        emptyTable,
        duplicateTransition,
        shadowedTransition,
        unreachableState,
        contradictoryRule,
        forbiddenTransition,
        disallowedTransition,
        unusedEvent,
        deadEndState,
        overriddenAnyRow,
        unusedAllowance,
        canReject
    };

    inline constexpr std::size_t findingKindCount = 12;

    enum class Severity : uint8_t
    {
        info,
        warning,
        error
    };

    constexpr Severity SeverityOf(FindingKind kind);
    constexpr const char* NameOf(FindingKind kind);
    constexpr const char* NameOf(Severity severity);

    template<class Machine>
    class TransitionTableAnalysis
    {
    public:
        using StateId = typename Machine::StateId;
        using EventId = typename Machine::EventId;
        using Transition = typename Machine::Transition;
        using Table = typename Machine::Table;
        using Rules = typename Machine::Rules;

        struct Finding
        {
            FindingKind kind;
            std::optional<std::size_t> row{};
            std::optional<std::size_t> otherRow{};
            std::optional<StateId> state{};
            std::optional<EventId> event{};
            std::optional<StateId> target{};
        };

        template<std::size_t N>
        constexpr explicit TransitionTableAnalysis(const std::array<Transition, N>& rows);
        explicit TransitionTableAnalysis(Table table);

        template<class... S>
        static constexpr Rules Terminal();

        constexpr std::size_t Size() const;
        constexpr const Transition& Row(std::size_t index) const;

        constexpr ConsistencyError CheckConsistency(StateId initial) const;
        constexpr bool IsValid(StateId initial, const Rules& rules = {}, Severity failAt = Severity::error) const;
        constexpr std::optional<Severity> HighestSeverity(StateId initial, const Rules& rules = {}) const;

        template<class F>
        constexpr void ForEachFinding(StateId initial, const Rules& rules, Severity minimum, F callback) const;

        constexpr bool HasTransition(StateId from, EventId event) const;
        constexpr bool HasUnguardedSpecificRow(StateId from, EventId event) const;

    private:
        using StateSet = std::array<bool, StateId::count>;
        using Edges = std::array<StateSet, StateId::count>;

        template<class F>
        constexpr void ForEachStructuralError(StateId initial, F& callback) const;
        template<class F>
        constexpr void ForEachRuleError(const Rules& rules, F& callback) const;
        template<class F>
        constexpr void ForEachWarning(const Rules& rules, F& callback) const;
        template<class F>
        constexpr void ForEachInfo(F& callback) const;

        template<class F>
        constexpr void ForEachRepeatedPair(F& callback) const;
        template<class F>
        constexpr void ForEachUnreachableState(StateId initial, F& callback) const;
        template<class F>
        constexpr void ForEachContradictoryRule(const Rules& rules, F& callback) const;
        template<class F>
        constexpr void ForEachRuleViolation(const Rules& rules, F& callback) const;
        template<class F>
        constexpr void ForEachUnusedEvent(F& callback) const;
        template<class F>
        constexpr void ForEachDeadEndState(const Rules& rules, F& callback) const;
        template<class F>
        constexpr void ForEachOverriddenAnyRow(F& callback) const;
        template<class F>
        constexpr void ForEachUnusedAllowance(const Rules& rules, F& callback) const;
        template<class F>
        constexpr void ForEachEdge(F callback) const;

        constexpr StateSet Reachable(StateId initial) const;
        constexpr Edges RealizedEdges() const;
        constexpr bool Leaves(std::size_t state) const;
        constexpr bool Handles(std::size_t event) const;
        constexpr bool IsOverriddenEverywhere(const Transition& anyRow) const;
        constexpr bool CanReject(std::size_t state, std::size_t event) const;
        static constexpr bool SamePair(const Transition& a, const Transition& b);
        static constexpr bool Enters(const Edges& edges, std::size_t to);

    private:
        const Transition* first;
        const Transition* last;
    };

    ////    Implementation    ////

    constexpr Severity SeverityOf(FindingKind kind)
    {
        switch (kind)
        {
            case FindingKind::unusedEvent:
            case FindingKind::deadEndState:
            case FindingKind::overriddenAnyRow:
            case FindingKind::unusedAllowance:
                return Severity::warning;
            case FindingKind::canReject:
                return Severity::info;
            default:
                return Severity::error;
        }
    }

    constexpr const char* NameOf(FindingKind kind)
    {
        constexpr std::array<const char*, findingKindCount> names{ "emptyTable", "duplicateTransition", "shadowedTransition", "unreachableState", "contradictoryRule", "forbiddenTransition",
            "disallowedTransition", "unusedEvent", "deadEndState", "overriddenAnyRow", "unusedAllowance", "canReject" };
        return names[static_cast<std::size_t>(kind)];
    }

    constexpr const char* NameOf(Severity severity)
    {
        constexpr std::array<const char*, 3> names{ "info", "warning", "error" };
        return names[static_cast<std::size_t>(severity)];
    }

    template<class Machine>
    template<std::size_t N>
    constexpr TransitionTableAnalysis<Machine>::TransitionTableAnalysis(const std::array<Transition, N>& rows)
        : first(rows.data())
        , last(rows.data() + N)
    {}

    template<class Machine>
    TransitionTableAnalysis<Machine>::TransitionTableAnalysis(Table table)
        : first(table.begin())
        , last(table.end())
    {}

    template<class Machine>
    template<class... S>
    constexpr typename TransitionTableAnalysis<Machine>::Rules TransitionTableAnalysis<Machine>::Terminal()
    {
        return Rules{}.template Terminal<S...>();
    }

    template<class Machine>
    constexpr std::size_t TransitionTableAnalysis<Machine>::Size() const
    {
        return static_cast<std::size_t>(last - first);
    }

    template<class Machine>
    constexpr const typename TransitionTableAnalysis<Machine>::Transition& TransitionTableAnalysis<Machine>::Row(std::size_t index) const
    {
        return first[index];
    }

    template<class Machine>
    constexpr ConsistencyError TransitionTableAnalysis<Machine>::CheckConsistency(StateId initial) const
    {
        std::optional<FindingKind> worst;
        auto record = [&worst](const Finding& finding)
        {
            if (!worst || finding.kind < *worst)
                worst = finding.kind;
        };
        ForEachStructuralError(initial, record);

        if (!worst)
            return ConsistencyError::none;

        constexpr std::array<ConsistencyError, 4> errors{ ConsistencyError::emptyTable, ConsistencyError::duplicateTransition, ConsistencyError::shadowedTransition, ConsistencyError::unreachableState };
        return errors[static_cast<std::size_t>(*worst)];
    }

    template<class Machine>
    constexpr bool TransitionTableAnalysis<Machine>::IsValid(StateId initial, const Rules& rules, Severity failAt) const
    {
        auto highest = HighestSeverity(initial, rules);
        return !highest || *highest < failAt;
    }

    template<class Machine>
    constexpr std::optional<Severity> TransitionTableAnalysis<Machine>::HighestSeverity(StateId initial, const Rules& rules) const
    {
        std::optional<Severity> highest;
        ForEachFinding(initial, rules, Severity::info, [&highest](const Finding& finding)
            {
                if (!highest || SeverityOf(finding.kind) > *highest)
                    highest = SeverityOf(finding.kind);
            });
        return highest;
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachFinding(StateId initial, const Rules& rules, Severity minimum, F callback) const
    {
        ForEachStructuralError(initial, callback);
        ForEachRuleError(rules, callback);

        if (Size() == 0)
            return;

        if (minimum <= Severity::warning)
            ForEachWarning(rules, callback);
        if (minimum <= Severity::info)
            ForEachInfo(callback);
    }

    template<class Machine>
    constexpr bool TransitionTableAnalysis<Machine>::HasTransition(StateId from, EventId event) const
    {
        for (auto row = first; row != last; ++row)
            if ((!row->from || *row->from == from.Index()) && row->event == event.Index())
                return true;

        return false;
    }

    template<class Machine>
    constexpr bool TransitionTableAnalysis<Machine>::HasUnguardedSpecificRow(StateId from, EventId event) const
    {
        for (auto row = first; row != last; ++row)
            if (row->from == from.Index() && row->event == event.Index() && !row->guarded)
                return true;

        return false;
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachStructuralError(StateId initial, F& callback) const
    {
        if (Size() == 0)
        {
            callback(Finding{ FindingKind::emptyTable });
            return;
        }

        ForEachRepeatedPair(callback);
        ForEachUnreachableState(initial, callback);
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachRuleError(const Rules& rules, F& callback) const
    {
        ForEachContradictoryRule(rules, callback);
        ForEachRuleViolation(rules, callback);
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachWarning(const Rules& rules, F& callback) const
    {
        ForEachUnusedEvent(callback);
        ForEachDeadEndState(rules, callback);
        ForEachOverriddenAnyRow(callback);
        ForEachUnusedAllowance(rules, callback);
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachInfo(F& callback) const
    {
        for (std::size_t state = 0; state != StateId::count; ++state)
            for (std::size_t event = 0; event != EventId::count; ++event)
                if (CanReject(state, event))
                    callback(Finding{ FindingKind::canReject, std::nullopt, std::nullopt, StateId::FromIndex(state), EventId::FromIndex(event) });
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachRepeatedPair(F& callback) const
    {
        for (std::size_t earlier = 0; earlier != Size(); ++earlier)
            for (std::size_t later = earlier + 1; later != Size(); ++later)
                if (SamePair(first[earlier], first[later]) && !first[earlier].guarded)
                {
                    auto kind = first[later].guarded ? FindingKind::shadowedTransition : FindingKind::duplicateTransition;
                    std::optional<StateId> state;
                    if (first[later].from)
                        state = StateId::FromIndex(*first[later].from);
                    callback(Finding{ kind, later, earlier, state, EventId::FromIndex(first[later].event) });
                }
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachUnreachableState(StateId initial, F& callback) const
    {
        auto reached = Reachable(initial);

        for (std::size_t state = 0; state != StateId::count; ++state)
            if (!reached[state])
                callback(Finding{ FindingKind::unreachableState, std::nullopt, std::nullopt, StateId::FromIndex(state) });
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachContradictoryRule(const Rules& rules, F& callback) const
    {
        for (std::size_t from = 0; from != StateId::count; ++from)
            for (std::size_t to = 0; to != StateId::count; ++to)
                if (rules.IsExplicitlyAllowed(from, to) && rules.IsForbidden(from, to))
                    callback(Finding{ FindingKind::contradictoryRule, std::nullopt, std::nullopt, StateId::FromIndex(from), std::nullopt, StateId::FromIndex(to) });
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachRuleViolation(const Rules& rules, F& callback) const
    {
        ForEachEdge([&rules, &callback](std::size_t row, std::size_t from, std::size_t to)
            {
                if (rules.IsForbidden(from, to))
                    callback(Finding{ FindingKind::forbiddenTransition, row, std::nullopt, StateId::FromIndex(from), std::nullopt, StateId::FromIndex(to) });
                else if (!rules.IsAllowed(from, to))
                    callback(Finding{ FindingKind::disallowedTransition, row, std::nullopt, StateId::FromIndex(from), std::nullopt, StateId::FromIndex(to) });
            });
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachUnusedEvent(F& callback) const
    {
        for (std::size_t event = 0; event != EventId::count; ++event)
            if (!Handles(event))
                callback(Finding{ FindingKind::unusedEvent, std::nullopt, std::nullopt, std::nullopt, EventId::FromIndex(event) });
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachDeadEndState(const Rules& rules, F& callback) const
    {
        for (std::size_t state = 0; state != StateId::count; ++state)
            if (!rules.IsTerminal(state) && !Leaves(state))
                callback(Finding{ FindingKind::deadEndState, std::nullopt, std::nullopt, StateId::FromIndex(state) });
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachOverriddenAnyRow(F& callback) const
    {
        for (std::size_t index = 0; index != Size(); ++index)
            if (!first[index].from && IsOverriddenEverywhere(first[index]))
                callback(Finding{ FindingKind::overriddenAnyRow, index, std::nullopt, std::nullopt, EventId::FromIndex(first[index].event) });
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachUnusedAllowance(const Rules& rules, F& callback) const
    {
        auto edges = RealizedEdges();

        for (std::size_t from = 0; from != StateId::count; ++from)
            for (std::size_t to = 0; to != StateId::count; ++to)
                if (rules.IsExplicitlyAllowed(from, to) && !edges[from][to])
                    callback(Finding{ FindingKind::unusedAllowance, std::nullopt, std::nullopt, StateId::FromIndex(from), std::nullopt, StateId::FromIndex(to) });

        for (std::size_t to = 0; to != StateId::count; ++to)
            if (rules.IsAllowedFromAny(to) && !Enters(edges, to))
                callback(Finding{ FindingKind::unusedAllowance, std::nullopt, std::nullopt, std::nullopt, std::nullopt, StateId::FromIndex(to) });
    }

    template<class Machine>
    template<class F>
    constexpr void TransitionTableAnalysis<Machine>::ForEachEdge(F callback) const
    {
        for (std::size_t index = 0; index != Size(); ++index)
        {
            const auto& row = first[index];

            if (row.internal)
                continue;

            if (row.from)
                callback(index, *row.from, row.to);
            else
                for (std::size_t from = 0; from != StateId::count; ++from)
                    if (!HasUnguardedSpecificRow(StateId::FromIndex(from), EventId::FromIndex(row.event)))
                        callback(index, from, row.to);
        }
    }

    template<class Machine>
    constexpr typename TransitionTableAnalysis<Machine>::StateSet TransitionTableAnalysis<Machine>::Reachable(StateId initial) const
    {
        StateSet reached{};
        reached[initial.Index()] = true;

        bool changed = true;
        while (changed)
        {
            changed = false;
            for (auto row = first; row != last; ++row)
                if (!reached[row->to] && (!row->from || reached[*row->from]))
                {
                    reached[row->to] = true;
                    changed = true;
                }
        }

        return reached;
    }

    template<class Machine>
    constexpr typename TransitionTableAnalysis<Machine>::Edges TransitionTableAnalysis<Machine>::RealizedEdges() const
    {
        Edges edges{};
        ForEachEdge([&edges](std::size_t, std::size_t from, std::size_t to)
            {
                edges[from][to] = true;
            });
        return edges;
    }

    template<class Machine>
    constexpr bool TransitionTableAnalysis<Machine>::Leaves(std::size_t state) const
    {
        for (auto row = first; row != last; ++row)
            if (!row->internal && (!row->from || *row->from == state) && row->to != state)
                return true;

        return false;
    }

    template<class Machine>
    constexpr bool TransitionTableAnalysis<Machine>::Handles(std::size_t event) const
    {
        for (auto row = first; row != last; ++row)
            if (row->event == event)
                return true;

        return false;
    }

    template<class Machine>
    constexpr bool TransitionTableAnalysis<Machine>::IsOverriddenEverywhere(const Transition& anyRow) const
    {
        for (std::size_t state = 0; state != StateId::count; ++state)
            if (!HasUnguardedSpecificRow(StateId::FromIndex(state), EventId::FromIndex(anyRow.event)))
                return false;

        return true;
    }

    template<class Machine>
    constexpr bool TransitionTableAnalysis<Machine>::CanReject(std::size_t state, std::size_t event) const
    {
        bool handled = false;

        for (auto row = first; row != last; ++row)
            if ((!row->from || *row->from == state) && row->event == event)
            {
                if (!row->guarded)
                    return false;
                handled = true;
            }

        return handled;
    }

    template<class Machine>
    constexpr bool TransitionTableAnalysis<Machine>::SamePair(const Transition& a, const Transition& b)
    {
        return a.from == b.from && a.event == b.event;
    }

    template<class Machine>
    constexpr bool TransitionTableAnalysis<Machine>::Enters(const Edges& edges, std::size_t to)
    {
        for (const auto& from : edges)
            if (from[to])
                return true;

        return false;
    }
}

#endif
