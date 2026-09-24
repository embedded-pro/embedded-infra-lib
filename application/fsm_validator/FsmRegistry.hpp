#ifndef APPLICATION_FSM_VALIDATOR_FSM_REGISTRY_HPP
#define APPLICATION_FSM_VALIDATOR_FSM_REGISTRY_HPP

#include "infra/stream/OutputStream.hpp"
#include "infra/util/IntrusiveList.hpp"
#include "services/fsm/StateMachineDefinition.hpp"
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

    template<class Definition>
    class FsmRegistrationFor
        : public FsmRegistration
    {
    public:
        using Machine = typename Definition::Machine;
        using Analysis = services::TransitionTableAnalysis<Machine>;

        explicit FsmRegistrationFor(const char* name, infra::IntrusiveList<FsmRegistration>& registrations = Registrations());

        std::optional<services::Severity> Validate(infra::TextOutputStream& stream, services::Severity minimum) const override;
        void WriteMermaid(infra::TextOutputStream& stream) const override;

    private:
        static Analysis Rows();
        static typename Machine::StateId Initial();
    };

    ////    Implementation    ////

    template<class Definition>
    FsmRegistrationFor<Definition>::FsmRegistrationFor(const char* name, infra::IntrusiveList<FsmRegistration>& registrations)
        : FsmRegistration(name, registrations)
    {
        static_assert(services::StateMachineDefinition<Definition>, "A state machine definition provides Machine, Initial, a constexpr Rows() and a constexpr Rules()");
    }

    template<class Definition>
    std::optional<services::Severity> FsmRegistrationFor<Definition>::Validate(infra::TextOutputStream& stream, services::Severity minimum) const
    {
        return services::WriteValidationReport(stream, Rows(), Initial(), Definition::Rules(), minimum);
    }

    template<class Definition>
    void FsmRegistrationFor<Definition>::WriteMermaid(infra::TextOutputStream& stream) const
    {
        services::WriteMermaid(stream, Rows(), Initial());
    }

    template<class Definition>
    typename FsmRegistrationFor<Definition>::Analysis FsmRegistrationFor<Definition>::Rows()
    {
        static constexpr auto rows = Definition::Rows();
        return Analysis(rows);
    }

    template<class Definition>
    typename FsmRegistrationFor<Definition>::Machine::StateId FsmRegistrationFor<Definition>::Initial()
    {
        return Machine::StateId::template Of<typename Definition::Initial>();
    }
}

#endif
