#include "application/fsm_validator/FsmValidator.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "gtest/gtest.h"

namespace
{
    struct Stopped
    {
        static constexpr const char* name{ "Stopped" };
    };

    struct Running
    {
        static constexpr const char* name{ "Running" };
    };

    struct Halted
    {
        static constexpr const char* name{ "Halted" };
    };

    using State = std::variant<Stopped, Running, Halted>;

    struct Go
    {
        static constexpr const char* name{ "Go" };
    };

    struct Halt
    {
        static constexpr const char* name{ "Halt" };
    };

    using Event = std::variant<Go, Halt>;

    struct Engine
    {};

    using Machine = services::TableStateMachine<State, Event, Engine>;

    constexpr std::array valid{
        Machine::Row<Stopped, Go, Running>(),
        Machine::Row<Running, Halt, Stopped>(),
        Machine::RowFromAny<Go, Halted>([](Engine&, const State&, const Go&)
            {
                return true;
            }),
        Machine::Row<Halted, Halt, Stopped>(),
    };

    constexpr std::array withDeadEnd{
        Machine::Row<Stopped, Go, Running>(),
        Machine::Row<Running, Halt, Halted>(),
    };

    constexpr std::array withDuplicate{
        Machine::Row<Stopped, Go, Running>(),
        Machine::Row<Stopped, Go, Halted>(),
        Machine::Row<Running, Halt, Stopped>(),
        Machine::Row<Halted, Halt, Stopped>(),
    };
}

class FsmValidatorTest
    : public testing::Test
{
public:
    infra::IntrusiveList<application::FsmRegistration> registrations;
    infra::StringOutputStream::WithStorage<2048> report;
    application::FsmValidator validator{ registrations, report };
    application::FsmRegistrationFor<Machine, Stopped> first{ "Valid", infra::MakeRange(valid), registrations };
    application::FsmRegistrationFor<Machine, Stopped> second{ "DeadEnd", infra::MakeRange(withDeadEnd), registrations };
};

TEST_F(FsmValidatorTest, list_writes_every_registered_name)
{
    validator.List();

    EXPECT_EQ("Valid\nDeadEnd\n", report.Storage());
}

TEST_F(FsmValidatorTest, warnings_are_reported_but_pass_by_default)
{
    EXPECT_TRUE(validator.Validate({}));

    EXPECT_EQ(
        "[Valid]\n"
        "[Valid] passed\n"
        "[DeadEnd]\n"
        "warning deadEndState: Halted has no transition to another state\n"
        "[DeadEnd] passed\n",
        report.Storage());
}

TEST_F(FsmValidatorTest, warnings_fail_in_strict_mode)
{
    application::FsmValidatorOptions options;
    options.failAt = services::Severity::warning;

    options.names = { "DeadEnd" };

    EXPECT_FALSE(validator.Validate(options));

    EXPECT_EQ(
        "[DeadEnd]\n"
        "warning deadEndState: Halted has no transition to another state\n"
        "[DeadEnd] failed\n",
        report.Storage());
}

TEST_F(FsmValidatorTest, info_findings_are_reported_on_request)
{
    application::FsmValidatorOptions options;
    options.minimum = services::Severity::info;
    options.names = { "Valid" };

    EXPECT_TRUE(validator.Validate(options));

    EXPECT_EQ(
        "[Valid]\n"
        "info canReject: Go in Running is rejected when every guard refuses\n"
        "info canReject: Go in Halted is rejected when every guard refuses\n"
        "[Valid] passed\n",
        report.Storage());
}

TEST_F(FsmValidatorTest, errors_fail_validation)
{
    application::FsmRegistrationFor<Machine, Stopped> duplicate{ "Duplicate", infra::MakeRange(withDuplicate), registrations };
    application::FsmValidatorOptions options;
    options.names = { "Duplicate" };

    EXPECT_FALSE(validator.Validate(options));

    EXPECT_EQ(
        "[Duplicate]\n"
        "error duplicateTransition: row 1 (Stopped --Go--> Halted) duplicates unguarded row 0 (Stopped --Go--> Running)\n"
        "[Duplicate] failed\n",
        report.Storage());
}

TEST_F(FsmValidatorTest, unknown_name_fails_validation)
{
    application::FsmValidatorOptions options;
    options.names = { "Missing" };

    EXPECT_FALSE(validator.Validate(options));
    EXPECT_EQ("[Missing] unknown state machine\n", report.Storage());
}

TEST_F(FsmValidatorTest, registration_is_removed_on_destruction)
{
    {
        application::FsmRegistrationFor<Machine, Stopped> temporary{ "Temporary", infra::MakeRange(valid), registrations };
        EXPECT_NE(nullptr, validator.Find("Temporary"));
    }

    EXPECT_EQ(nullptr, validator.Find("Temporary"));
}

TEST_F(FsmValidatorTest, registration_writes_mermaid)
{
    first.WriteMermaid(report);

    EXPECT_EQ(
        "stateDiagram-v2\n"
        "    [*] --> Stopped\n"
        "    Stopped --> Running : Go\n"
        "    Running --> Stopped : Halt\n"
        "    Running --> Halted : Go [guarded]\n"
        "    Halted --> Halted : Go [guarded]\n"
        "    Halted --> Stopped : Halt\n",
        report.Storage());
}
