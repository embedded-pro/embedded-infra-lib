#ifndef SERVICES_FSM_TEST_COMPILE_FAIL_TOGGLE_HPP
#define SERVICES_FSM_TEST_COMPILE_FAIL_TOGGLE_HPP

#include "services/fsm/StateMachineDefinition.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include <array>

namespace compile_fail
{
    struct Idle
    {
        static constexpr const char* name{ "Idle" };
    };

    struct Running
    {
        static constexpr const char* name{ "Running" };
    };

    using State = std::variant<Idle, Running>;

    struct Start
    {
        static constexpr const char* name{ "Start" };
    };

    struct Stop
    {
        static constexpr const char* name{ "Stop" };
    };

    struct Pause
    {
        static constexpr const char* name{ "Pause" };
    };

    using Event = std::variant<Start, Stop, Pause>;

    struct Toggle
    {
        using Machine = services::TableStateMachine<State, Event, Toggle>;
        using Initial = Idle;

        static constexpr std::array<Machine::Transition, 2> CoreRows()
        {
            return { Machine::Row<Idle, Start, Running>(), Machine::Row<Running, Stop, Idle>() };
        }

        static constexpr auto Rows()
        {
            return services::JoinRows(CoreRows(), std::array{ Machine::InternalRow<Running, Pause>() });
        }

        static constexpr Machine::Rules Rules()
        {
            return Machine::Rules{}.Allow<Idle, Running>().Allow<Running, Idle>();
        }
    };
}

#endif
