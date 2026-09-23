#ifndef APPLICATION_FSM_VALIDATOR_FSM_REGISTRY_HPP
#define APPLICATION_FSM_VALIDATOR_FSM_REGISTRY_HPP

#include "infra/stream/OutputStream.hpp"
#include "infra/util/IntrusiveList.hpp"
#include "services/fsm/StateMachineMermaid.hpp"
#include "services/fsm/StateMachineValidation.hpp"
#include "services/fsm/TransitionTableAnalysis.hpp"
#include <optional>

namespace application
{
    class FsmRegistration
        : public infra::IntrusiveList<FsmRegistration>::NodeType
    {
    public:
        explicit FsmRegistration(const char* name, infra::IntrusiveList<FsmRegistration>& registrations = Registrations());
        FsmRegistration(const FsmRegistration& other) = delete;
        FsmRegistration& operator=(const FsmRegistration& other) = delete;

        static infra::IntrusiveList<FsmRegistration>& Registrations();

        const char* Name() const;

        virtual std::optional<services::Severity> Validate(infra::TextOutputStream& stream, services::Severity minimum) const = 0;
        virtual void WriteMermaid(infra::TextOutputStream& stream) const = 0;

    protected:
        ~FsmRegistration();

    private:
        const char* name;
        infra::IntrusiveList<FsmRegistration>& registrations;
    };

    template<class Machine, class Initial, class... Terminals>
    class FsmRegistrationFor
        : public FsmRegistration
    {
    public:
        using Analysis = services::TransitionTableAnalysis<Machine>;

        FsmRegistrationFor(const char* name, typename Machine::Table table, infra::IntrusiveList<FsmRegistration>& registrations = Registrations());

        std::optional<services::Severity> Validate(infra::TextOutputStream& stream, services::Severity minimum) const override;
        void WriteMermaid(infra::TextOutputStream& stream) const override;

    private:
        typename Machine::Table table;
    };

    ////    Implementation    ////

    template<class Machine, class Initial, class... Terminals>
    FsmRegistrationFor<Machine, Initial, Terminals...>::FsmRegistrationFor(const char* name, typename Machine::Table table, infra::IntrusiveList<FsmRegistration>& registrations)
        : FsmRegistration(name, registrations)
        , table(table)
    {}

    template<class Machine, class Initial, class... Terminals>
    std::optional<services::Severity> FsmRegistrationFor<Machine, Initial, Terminals...>::Validate(infra::TextOutputStream& stream, services::Severity minimum) const
    {
        return services::WriteValidationReport(stream, Analysis(table), Machine::StateId::template Of<Initial>(), Analysis::template Terminal<Terminals...>(), minimum);
    }

    template<class Machine, class Initial, class... Terminals>
    void FsmRegistrationFor<Machine, Initial, Terminals...>::WriteMermaid(infra::TextOutputStream& stream) const
    {
        services::WriteMermaid(stream, Analysis(table), Machine::StateId::template Of<Initial>());
    }
}

#endif
