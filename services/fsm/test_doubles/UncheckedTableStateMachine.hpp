#ifndef SERVICES_UNCHECKED_TABLE_STATE_MACHINE_HPP
#define SERVICES_UNCHECKED_TABLE_STATE_MACHINE_HPP

#include "services/fsm/TableStateMachine.hpp"

namespace services
{
    template<class State, class Event, class Context>
    struct UncheckedTableStateMachine
    {
        using Machine = TableStateMachine<State, Event, Context>;

        template<std::size_t QueueDepth>
        class WithStorage
            : public Machine::template WithStorage<QueueDepth>
        {
        public:
            WithStorage(Context& context, typename Machine::Table table);
        };
    };

    ////    Implementation    ////

    template<class State, class Event, class Context>
    template<std::size_t QueueDepth>
    UncheckedTableStateMachine<State, Event, Context>::WithStorage<QueueDepth>::WithStorage(Context& context, typename Machine::Table table)
        : Machine::template WithStorage<QueueDepth>(context, table)
    {}
}

#endif
