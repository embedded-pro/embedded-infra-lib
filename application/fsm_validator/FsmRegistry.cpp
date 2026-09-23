#include "application/fsm_validator/FsmRegistry.hpp"

namespace application
{
    FsmRegistration::FsmRegistration(const char* name, infra::IntrusiveList<FsmRegistration>& registrations)
        : name(name)
        , registrations(registrations)
    {
        registrations.push_back(*this);
    }

    FsmRegistration::~FsmRegistration()
    {
        registrations.erase(*this);
    }

    infra::IntrusiveList<FsmRegistration>& FsmRegistration::Registrations()
    {
        static infra::IntrusiveList<FsmRegistration> registrations;
        return registrations;
    }

    const char* FsmRegistration::Name() const
    {
        return name;
    }
}
