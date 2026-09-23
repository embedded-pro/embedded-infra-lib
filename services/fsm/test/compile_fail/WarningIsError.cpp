#include "services/fsm/test/compile_fail/Toggle.hpp"

namespace
{
    struct WithoutPause
        : compile_fail::Toggle
    {
        static constexpr auto Rows()
        {
#ifdef EMIL_FSM_COMPILE_FAIL
            return CoreRows();
#else
            return Toggle::Rows();
#endif
        }
    };
}

void Validate()
{
    services::Validated<WithoutPause>();
}
