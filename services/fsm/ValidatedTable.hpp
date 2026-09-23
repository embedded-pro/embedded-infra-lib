#ifndef SERVICES_VALIDATED_TABLE_HPP
#define SERVICES_VALIDATED_TABLE_HPP

#include "infra/util/MemoryRange.hpp"
#include <cstddef>

namespace services
{
    template<class Machine>
    class ValidatedTable;

    template<class Definition>
    ValidatedTable<typename Definition::Machine> Validated();

    template<class Machine>
    class ValidatedTable
    {
    public:
        using Transition = typename Machine::Transition;
        using StateId = typename Machine::StateId;

        infra::MemoryRange<const Transition> Table() const;
        StateId Initial() const;

    private:
        ValidatedTable(infra::MemoryRange<const Transition> table, StateId initial);

        template<class Definition>
        friend ValidatedTable<typename Definition::Machine> Validated();

    private:
        infra::MemoryRange<const Transition> table;
        StateId initial;
    };

    ////    Implementation    ////

    template<class Machine>
    ValidatedTable<Machine>::ValidatedTable(infra::MemoryRange<const Transition> table, StateId initial)
        : table(table)
        , initial(initial)
    {}

    template<class Machine>
    infra::MemoryRange<const typename ValidatedTable<Machine>::Transition> ValidatedTable<Machine>::Table() const
    {
        return table;
    }

    template<class Machine>
    typename ValidatedTable<Machine>::StateId ValidatedTable<Machine>::Initial() const
    {
        return initial;
    }
}

#endif
