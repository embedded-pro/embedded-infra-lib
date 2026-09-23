#include "infra/stream/StringOutputStream.hpp"
#include "services/fsm/StateMachineMermaid.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    struct Idle
    {
        static constexpr const char* name{ "Idle" };
    };

    struct Running
    {
        static constexpr const char* name{ "Running" };
    };

    struct Fault
    {
        static constexpr const char* name{ "Fault" };
    };

    using State = std::variant<Idle, Running, Fault>;

    struct Start
    {
        static constexpr const char* name{ "Start" };
    };

    struct Stop
    {
        static constexpr const char* name{ "Stop" };
    };

    struct Error
    {
        static constexpr const char* name{ "Error" };
    };

    struct Tick
    {
        static constexpr const char* name{ "Tick" };
    };

    using Event = std::variant<Start, Stop, Error, Tick>;

    struct Engine
    {};

    using Machine = services::TableStateMachine<State, Event, Engine>;

    constexpr std::array rows{
        Machine::Row<Idle, Start, Running>(),
        Machine::Row<Running, Stop, Idle>([](Engine&, const Running&, const Stop&)
            {
                return true;
            }),
        Machine::InternalRow<Running, Tick>(),
        Machine::RowFromAny<Error, Fault>(),
        Machine::Row<Fault, Stop, Idle>(),
    };
}

TEST(StateMachineMermaidTest, mermaid_output_lists_initial_state_and_every_row)
{
    Engine engine;
    Machine::WithStorage<1> fsm{ engine, infra::MakeRange(rows) };

    infra::StringOutputStream::WithStorage<512> stream;
    services::WriteMermaid(stream, fsm, services::AlternativeId<State>::Of<Idle>());

    EXPECT_EQ(
        "stateDiagram-v2\n"
        "    [*] --> Idle\n"
        "    Idle --> Running : Start\n"
        "    Running --> Idle : Stop [guarded]\n"
        "    Running --> Running : Tick (internal)\n"
        "    Idle --> Fault : Error\n"
        "    Running --> Fault : Error\n"
        "    Fault --> Fault : Error\n"
        "    Fault --> Idle : Stop\n",
        stream.Storage());
}

TEST(StateMachineMermaidTest, any_state_edge_is_omitted_where_unguarded_specific_row_wins)
{
    static constexpr std::array table{
        Machine::Row<Idle, Start, Running>(),
        Machine::Row<Running, Error, Idle>(),
        Machine::RowFromAny<Error, Fault>(),
        Machine::Row<Fault, Stop, Idle>(),
    };

    infra::StringOutputStream::WithStorage<512> stream;
    services::WriteMermaid(stream, services::TransitionTableAnalysis<Machine>(table), services::AlternativeId<State>::Of<Idle>());

    EXPECT_EQ(
        "stateDiagram-v2\n"
        "    [*] --> Idle\n"
        "    Idle --> Running : Start\n"
        "    Running --> Idle : Error\n"
        "    Idle --> Fault : Error\n"
        "    Fault --> Fault : Error\n"
        "    Fault --> Idle : Stop\n",
        stream.Storage());
}
