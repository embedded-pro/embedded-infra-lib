#include "services/fsm/test/compile_fail/Toggle.hpp"

namespace
{
    struct WithSelfTransition
        : compile_fail::Toggle
    {
        static constexpr auto Rows()
        {
#ifdef EMIL_FSM_COMPILE_FAIL
            return services::JoinRows(Toggle::Rows(), std::array{ Machine::Row<compile_fail::Running, compile_fail::Start, compile_fail::Running>() });
#else
            return Toggle::Rows();
#endif
        }
    };
}

void Validate()
{
    services::Validated<WithSelfTransition>();
}
