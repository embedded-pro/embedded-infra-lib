#include "application/fsm_validator/FsmRegistry.hpp"
#include "services/fsm/test/JobLifecycle.hpp"

namespace
{
    constexpr auto jobLifecycleRows = example::JobLifecycle::Rows();

    application::FsmRegistrationFor<example::JobLifecycle::Machine, example::Idle> jobLifecycle{ "JobLifecycle", infra::MakeRange(jobLifecycleRows) };
}
