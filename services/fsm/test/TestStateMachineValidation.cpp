#include "infra/stream/StringOutputStream.hpp"
#include "services/fsm/StateMachineValidation.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "gtest/gtest.h"

namespace
{
    struct Closed
    {
        static constexpr const char* name{ "Closed" };
    };

    struct Open
    {
        static constexpr const char* name{ "Open" };
    };

    struct Jammed
    {
        static constexpr const char* name{ "Jammed" };
    };

    using State = std::variant<Closed, Open, Jammed>;

    struct Push
    {
        static constexpr const char* name{ "Push" };
    };

    struct Pull
    {
        static constexpr const char* name{ "Pull" };
    };

    struct Kick
    {
        static constexpr const char* name{ "Kick" };
    };

    using Event = std::variant<Push, Pull, Kick>;

    struct Door
    {};

    using StateId = services::AlternativeId<State>;
    using Machine = services::TableStateMachine<State, Event, Door>;
    using Analysis = services::TransitionTableAnalysis<Machine>;

    constexpr auto accept = [](Door&, const auto&, const auto&)
    {
        return true;
    };

    constexpr std::array valid{
        Machine::Row<Closed, Push, Open>(),
        Machine::Row<Open, Pull, Closed>(),
        Machine::RowFromAny<Kick, Jammed>(),
        Machine::Row<Jammed, Pull, Closed>(),
    };

    constexpr std::array broken{
        Machine::Row<Closed, Push, Open>(),
        Machine::Row<Closed, Push, Open>(accept),
        Machine::Row<Open, Pull, Closed>(accept),
        Machine::Row<Open, Kick, Closed>(),
    };

    constexpr std::array<Machine::Transition, 0> empty{};
}

class StateMachineValidationTest
    : public testing::Test
{
public:
    std::optional<services::Severity> Report(const Analysis& analysis, services::Severity minimum = services::Severity::info)
    {
        return services::WriteValidationReport(stream, analysis, StateId::Of<Closed>(), {}, minimum);
    }

    infra::StringOutputStream::WithStorage<1024> stream;
};

TEST_F(StateMachineValidationTest, valid_table_writes_nothing)
{
    EXPECT_EQ(std::nullopt, Report(Analysis(valid)));
    EXPECT_EQ("", stream.Storage());
}

TEST_F(StateMachineValidationTest, empty_table_is_reported)
{
    EXPECT_EQ(services::Severity::error, Report(Analysis(empty)));
    EXPECT_EQ("error emptyTable: the table has no rows\n", stream.Storage());
}

TEST_F(StateMachineValidationTest, every_finding_is_written_with_rows_and_names)
{
    EXPECT_EQ(services::Severity::error, Report(Analysis(broken)));

    EXPECT_EQ(
        "error shadowedTransition: row 1 (Closed --Push--> Open [guarded]) follows unguarded row 0 (Closed --Push--> Open) and is never selected\n"
        "error unreachableState: Jammed is not reachable from Closed\n"
        "warning deadEndState: Jammed has no transition to another state\n"
        "info canReject: Pull in Open is rejected when every guard refuses\n",
        stream.Storage());
}

TEST_F(StateMachineValidationTest, duplicate_unused_and_overridden_rows_are_written)
{
    static constexpr std::array table{
        Machine::Row<Closed, Push, Open>(),
        Machine::Row<Closed, Push, Open>(),
        Machine::Row<Open, Push, Jammed>(),
        Machine::RowFromAny<Push, Closed>(),
        Machine::Row<Jammed, Push, Closed>(),
    };

    EXPECT_EQ(services::Severity::error, Report(Analysis(table)));

    EXPECT_EQ(
        "error duplicateTransition: row 1 (Closed --Push--> Open) duplicates unguarded row 0 (Closed --Push--> Open)\n"
        "warning unusedEvent: Pull is handled by no row\n"
        "warning unusedEvent: Kick is handled by no row\n"
        "warning overriddenAnyRow: row 3 (* --Push--> Closed) is overridden by an unguarded row in every state\n",
        stream.Storage());
}

TEST_F(StateMachineValidationTest, minimum_severity_limits_report)
{
    EXPECT_EQ(services::Severity::error, Report(Analysis(broken), services::Severity::error));

    EXPECT_EQ(
        "error shadowedTransition: row 1 (Closed --Push--> Open [guarded]) follows unguarded row 0 (Closed --Push--> Open) and is never selected\n"
        "error unreachableState: Jammed is not reachable from Closed\n",
        stream.Storage());
}

TEST_F(StateMachineValidationTest, internal_row_is_marked_in_report)
{
    static constexpr auto table = services::JoinRows(valid, std::array{ Machine::InternalRow<Open, Push>(), Machine::InternalRow<Open, Push>() });

    Report(Analysis(table), services::Severity::error);

    EXPECT_EQ("error duplicateTransition: row 5 (Open --Push--> Open (internal)) duplicates unguarded row 4 (Open --Push--> Open (internal))\n", stream.Storage());
}
