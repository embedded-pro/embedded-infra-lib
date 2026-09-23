#ifndef APPLICATION_FSM_VALIDATOR_FSM_VALIDATOR_HPP
#define APPLICATION_FSM_VALIDATOR_FSM_VALIDATOR_HPP

#include "application/fsm_validator/FsmRegistry.hpp"
#include <string>
#include <vector>

namespace application
{
    struct FsmValidatorOptions
    {
        services::Severity minimum{ services::Severity::warning };
        services::Severity failAt{ services::Severity::error };
        std::vector<std::string> names;
    };

    class FsmValidator
    {
    public:
        FsmValidator(infra::IntrusiveList<FsmRegistration>& registrations, infra::TextOutputStream& report);

        void List();
        bool Validate(const FsmValidatorOptions& options);
        const FsmRegistration* Find(const std::string& name) const;

    private:
        bool ValidateOne(const FsmRegistration& registration, const FsmValidatorOptions& options);
        bool ValidateSelected(const FsmValidatorOptions& options);

    private:
        infra::IntrusiveList<FsmRegistration>& registrations;
        infra::TextOutputStream& report;
    };
}

#endif
