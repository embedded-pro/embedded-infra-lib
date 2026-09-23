#include "application/fsm_validator/FsmValidator.hpp"

namespace application
{
    FsmValidator::FsmValidator(infra::IntrusiveList<FsmRegistration>& registrations, infra::TextOutputStream& report)
        : registrations(registrations)
        , report(report)
    {}

    void FsmValidator::List()
    {
        for (const auto& registration : registrations)
            report << registration.Name() << "\n";
    }

    bool FsmValidator::Validate(const FsmValidatorOptions& options)
    {
        if (!options.names.empty())
            return ValidateSelected(options);

        bool passed = true;
        for (const auto& registration : registrations)
            passed = ValidateOne(registration, options) && passed;

        return passed;
    }

    const FsmRegistration* FsmValidator::Find(const std::string& name) const
    {
        for (const auto& registration : registrations)
            if (name == registration.Name())
                return &registration;

        return nullptr;
    }

    bool FsmValidator::ValidateOne(const FsmRegistration& registration, const FsmValidatorOptions& options)
    {
        report << "[" << registration.Name() << "]\n";
        auto highest = registration.Validate(report, options.minimum);
        bool passed = !highest || *highest < options.failAt;
        report << "[" << registration.Name() << "] " << (passed ? "passed" : "failed") << "\n";
        return passed;
    }

    bool FsmValidator::ValidateSelected(const FsmValidatorOptions& options)
    {
        bool passed = true;

        for (const auto& name : options.names)
        {
            const auto* registration = Find(name);
            if (registration != nullptr)
                passed = ValidateOne(*registration, options) && passed;
            else
            {
                report << "[" << name << "] unknown state machine\n";
                passed = false;
            }
        }

        return passed;
    }
}
