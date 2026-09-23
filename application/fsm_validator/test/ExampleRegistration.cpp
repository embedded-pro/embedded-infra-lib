#include "application/fsm_validator/FsmRegistry.hpp"
#include "services/fsm/test/JobLifecycle.hpp"

namespace
{
    application::FsmRegistrationFor<example::JobLifecycle> jobLifecycle{ "JobLifecycle" };
}
