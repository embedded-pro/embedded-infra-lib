#include "services/fsm/test/CompileFailToggle.hpp"

void Construct(compile_fail::Toggle& toggle)
{
#ifdef EMIL_FSM_COMPILE_FAIL
    static constexpr auto rows = compile_fail::Toggle::Rows();
    compile_fail::Toggle::Machine::WithStorage<1> machine{ toggle, infra::MakeRange(rows) };
#else
    compile_fail::Toggle::Machine::WithStorage<1> machine{ toggle, services::Validated<compile_fail::Toggle>() };
#endif
    machine.Start<compile_fail::Idle>();
}
