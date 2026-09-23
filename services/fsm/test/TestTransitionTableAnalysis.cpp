#include "infra/util/BoundedVector.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "services/fsm/TransitionTableAnalysis.hpp"
#include "gtest/gtest.h"

namespace
{
    struct Off
    {
        static constexpr const char* name{ "Off" };
    };

    struct On
    {
        static constexpr const char* name{ "On" };
    };

    struct Broken
    {
        static constexpr const char* name{ "Broken" };
    };

    using State = std::variant<Off, On, Broken>;

    struct Press
    {
        static constexpr const char* name{ "Press" };
    };

    struct Break
    {
        static constexpr const char* name{ "Break" };
    };

    struct Fix
    {
        static constexpr const char* name{ "Fix" };
    };

    struct Lock
    {
        static constexpr const char* name{ "Lock" };
    };

    using Event = std::variant<Press, Break, Fix, Lock>;

    struct Lamp
    {};

    using StateId = services::AlternativeId<State>;
    using EventId = services::AlternativeId<Event>;
    using Machine = services::TableStateMachine<State, Event, Lamp>;
    using Analysis = services::TransitionTableAnalysis<Machine>;
    using Finding = Analysis::Finding;

    constexpr auto accept = [](Lamp&, const auto&, const auto&)
    {
        return true;
    };

    constexpr std::array core{
        Machine::Row<Off, Press, On>(),
        Machine::Row<On, Press, Off>(),
        Machine::RowFromAny<Break, Broken>(),
        Machine::Row<Broken, Fix, Off>(),
    };

    constexpr std::array lockRow{
        Machine::InternalRow<On, Lock>(),
    };

    constexpr auto baseline = services::JoinRows(core, lockRow);
    constexpr std::array<Machine::Transition, 0> empty{};

    constexpr auto duplicate = services::JoinRows(baseline, std::array{ Machine::Row<Off, Press, Broken>() });
    constexpr auto shadowed = services::JoinRows(baseline, std::array{ Machine::Row<Off, Press, Broken>(accept) });
    constexpr auto guardedPair = services::JoinRows(baseline, std::array{ Machine::Row<Off, Fix, On>(accept) });
    constexpr auto guardedWithFallback = services::JoinRows(baseline, std::array{ Machine::Row<Off, Break, On>(accept) });

    constexpr std::array unreachable{
        Machine::Row<Off, Press, On>(),
        Machine::Row<On, Press, Off>(),
        Machine::Row<Broken, Fix, Off>(),
        Machine::InternalRow<On, Lock>(),
    };

    constexpr std::array deadEnd{
        Machine::Row<Off, Press, On>(),
        Machine::Row<On, Press, Off>(),
        Machine::RowFromAny<Break, Broken>(),
        Machine::InternalRow<Broken, Fix>(),
        Machine::InternalRow<On, Lock>(),
    };

    constexpr auto overridden = services::JoinRows(baseline, std::array{
                                                                 Machine::Row<Off, Break, Broken>(),
                                                                 Machine::Row<On, Break, Broken>(),
                                                                 Machine::InternalRow<Broken, Break>(),
                                                             });

    template<class T>
    constexpr StateId Id()
    {
        return StateId::Of<T>();
    }

    template<class E>
    constexpr EventId Ev()
    {
        return EventId::Of<E>();
    }

    infra::BoundedVector<Finding>::WithMaxSize<16> FindingsOf(const Analysis& analysis, services::Severity minimum, const Analysis::TerminalStates& terminal = {})
    {
        infra::BoundedVector<Finding>::WithMaxSize<16> findings;
        analysis.ForEachFinding(Id<Off>(), terminal, minimum, [&findings](const Finding& finding)
            {
                findings.push_back(finding);
            });
        return findings;
    }

    static_assert(Analysis(baseline).IsValid(Id<Off>(), {}, services::Severity::info));
    static_assert(Analysis(duplicate).CheckConsistency(Id<Off>()) == services::ConsistencyError::duplicateTransition);
    static_assert(!Analysis(deadEnd).IsValid(Id<Off>(), {}, services::Severity::warning));
    static_assert(Analysis(deadEnd).IsValid(Id<Off>(), Analysis::Terminal<Broken>(), services::Severity::warning));
}

TEST(TransitionTableAnalysisTest, valid_table_has_no_findings)
{
    EXPECT_TRUE(FindingsOf(Analysis(baseline), services::Severity::info).empty());
    EXPECT_EQ(services::ConsistencyError::none, Analysis(baseline).CheckConsistency(Id<Off>()));
    EXPECT_EQ(std::nullopt, Analysis(baseline).HighestSeverity(Id<Off>()));
}

TEST(TransitionTableAnalysisTest, empty_table_reports_only_empty_table)
{
    auto findings = FindingsOf(Analysis(empty), services::Severity::info);

    ASSERT_EQ(1, findings.size());
    EXPECT_EQ(services::FindingKind::emptyTable, findings[0].kind);
    EXPECT_EQ(services::ConsistencyError::emptyTable, Analysis(empty).CheckConsistency(Id<Off>()));
}

TEST(TransitionTableAnalysisTest, duplicate_reports_both_rows_state_and_event)
{
    auto findings = FindingsOf(Analysis(duplicate), services::Severity::error);

    ASSERT_EQ(1, findings.size());
    EXPECT_EQ(services::FindingKind::duplicateTransition, findings[0].kind);
    EXPECT_EQ(5, findings[0].row);
    EXPECT_EQ(0, findings[0].otherRow);
    EXPECT_EQ(Id<Off>(), findings[0].state);
    EXPECT_EQ(Ev<Press>(), findings[0].event);
}

TEST(TransitionTableAnalysisTest, guarded_row_after_unguarded_row_is_shadowed)
{
    auto findings = FindingsOf(Analysis(shadowed), services::Severity::error);

    ASSERT_EQ(1, findings.size());
    EXPECT_EQ(services::FindingKind::shadowedTransition, findings[0].kind);
    EXPECT_EQ(5, findings[0].row);
    EXPECT_EQ(0, findings[0].otherRow);
    EXPECT_EQ(services::ConsistencyError::shadowedTransition, Analysis(shadowed).CheckConsistency(Id<Off>()));
}

TEST(TransitionTableAnalysisTest, unreachable_state_is_named)
{
    auto findings = FindingsOf(Analysis(unreachable), services::Severity::error);

    ASSERT_EQ(1, findings.size());
    EXPECT_EQ(services::FindingKind::unreachableState, findings[0].kind);
    EXPECT_EQ(Id<Broken>(), findings[0].state);
    EXPECT_EQ(services::ConsistencyError::unreachableState, Analysis(unreachable).CheckConsistency(Id<Off>()));
}

TEST(TransitionTableAnalysisTest, event_without_row_is_unused)
{
    auto findings = FindingsOf(Analysis(core), services::Severity::warning);

    ASSERT_EQ(1, findings.size());
    EXPECT_EQ(services::FindingKind::unusedEvent, findings[0].kind);
    EXPECT_EQ(Ev<Lock>(), findings[0].event);
}

TEST(TransitionTableAnalysisTest, state_without_exit_is_dead_end_unless_terminal)
{
    auto findings = FindingsOf(Analysis(deadEnd), services::Severity::warning);

    ASSERT_EQ(1, findings.size());
    EXPECT_EQ(services::FindingKind::deadEndState, findings[0].kind);
    EXPECT_EQ(Id<Broken>(), findings[0].state);
    EXPECT_TRUE(FindingsOf(Analysis(deadEnd), services::Severity::warning, Analysis::Terminal<Broken>()).empty());
}

TEST(TransitionTableAnalysisTest, any_state_row_overridden_in_every_state_is_reported)
{
    auto findings = FindingsOf(Analysis(overridden), services::Severity::warning);

    ASSERT_EQ(1, findings.size());
    EXPECT_EQ(services::FindingKind::overriddenAnyRow, findings[0].kind);
    EXPECT_EQ(2, findings[0].row);
    EXPECT_EQ(Ev<Break>(), findings[0].event);
}

TEST(TransitionTableAnalysisTest, pair_with_only_guarded_rows_can_reject)
{
    auto findings = FindingsOf(Analysis(guardedPair), services::Severity::info);

    ASSERT_EQ(1, findings.size());
    EXPECT_EQ(services::FindingKind::canReject, findings[0].kind);
    EXPECT_EQ(Id<Off>(), findings[0].state);
    EXPECT_EQ(Ev<Fix>(), findings[0].event);
    EXPECT_EQ(services::Severity::info, Analysis(guardedPair).HighestSeverity(Id<Off>()));
}

TEST(TransitionTableAnalysisTest, unguarded_any_state_row_is_fallback_for_guarded_specific_row)
{
    EXPECT_TRUE(FindingsOf(Analysis(guardedWithFallback), services::Severity::info).empty());
}

TEST(TransitionTableAnalysisTest, minimum_severity_filters_findings)
{
    EXPECT_TRUE(FindingsOf(Analysis(guardedPair), services::Severity::warning).empty());
    EXPECT_TRUE(FindingsOf(Analysis(core), services::Severity::error).empty());
}

TEST(TransitionTableAnalysisTest, is_valid_fails_at_requested_severity)
{
    EXPECT_TRUE(Analysis(core).IsValid(Id<Off>()));
    EXPECT_FALSE(Analysis(core).IsValid(Id<Off>(), {}, services::Severity::warning));
    EXPECT_FALSE(Analysis(duplicate).IsValid(Id<Off>()));
}

TEST(TransitionTableAnalysisTest, consistency_error_prefers_first_kind)
{
    static constexpr auto table = services::JoinRows(unreachable, std::array{ Machine::Row<Off, Press, Off>() });

    EXPECT_EQ(services::ConsistencyError::duplicateTransition, Analysis(table).CheckConsistency(Id<Off>()));
}

TEST(TransitionTableAnalysisTest, analysis_of_table_range_matches_analysis_of_array)
{
    Analysis fromRange{ infra::MakeRange(duplicate) };

    EXPECT_EQ(duplicate.size(), fromRange.Size());
    EXPECT_EQ(services::ConsistencyError::duplicateTransition, fromRange.CheckConsistency(Id<Off>()));
}

TEST(TransitionTableAnalysisTest, has_transition_and_unguarded_specific_row)
{
    Analysis analysis{ guardedWithFallback };

    EXPECT_TRUE(analysis.HasTransition(Id<Off>(), Ev<Break>()));
    EXPECT_FALSE(analysis.HasUnguardedSpecificRow(Id<Off>(), Ev<Break>()));
    EXPECT_TRUE(analysis.HasUnguardedSpecificRow(Id<Off>(), Ev<Press>()));
    EXPECT_FALSE(analysis.HasTransition(Id<Off>(), Ev<Lock>()));
}
