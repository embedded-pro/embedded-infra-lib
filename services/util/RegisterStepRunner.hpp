#ifndef SERVICES_REGISTER_STEP_RUNNER_HPP
#define SERVICES_REGISTER_STEP_RUNNER_HPP

#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/SharedPtr.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <cstdint>
#include <variant>

namespace services
{
    class RegisterStepRunner
    {
    public:
        using Action = infra::Function<void(), sizeof(void*)>;

        struct WriteRegister
        {
            uint8_t address;
            uint8_t value;
        };

        struct WriteBurst
        {
            uint8_t address;
            infra::ConstByteRange data;
        };

        struct ReadBurst
        {
            uint8_t address;
            infra::ByteRange data;
        };

        struct ModifyRegister
        {
            uint8_t address;
            uint8_t clearMask;
            uint8_t setMask;
        };

        struct Delay
        {
            infra::Duration duration;
        };

        // infra::Function has no move constructor, so its copy constructor is used to move an
        // action. Moving an action never throws, which these declarations make explicit.
        struct ActionStep
        {
            ActionStep(const Action& action)
                : action(action)
            {}

            ActionStep(const ActionStep& other) = default;
            ActionStep& operator=(const ActionStep& other) = default;
            ~ActionStep() = default;

            ActionStep(ActionStep&& other) noexcept
                : action(other.action)
            {}

            ActionStep& operator=(ActionStep&& other) noexcept
            {
                action = other.action;
                return *this;
            }

            Action action;
        };

        struct Invoke
            : ActionStep
        {
            using ActionStep::ActionStep;
        };

        struct Await
            : ActionStep
        {
            using ActionStep::ActionStep;
        };

        using Step = std::variant<WriteRegister, WriteBurst, ReadBurst, ModifyRegister, Delay, Invoke, Await>;

        static constexpr std::size_t maxSteps = 20;

        RegisterStepRunner(RegisterBusAccess& bus, infra::AccessedBySharedPtr& sharedAccess);
        RegisterStepRunner(const RegisterStepRunner& other) = delete;
        RegisterStepRunner& operator=(const RegisterStepRunner& other) = delete;

        void Clear();
        void Push(const Step& step);
        void Start(const infra::Function<void()>& onDone);

        void Continue();
        void Abort();
        bool Busy() const;

    private:
        void ExecuteCurrentStep();
        void Advance();
        void Complete();
        infra::Function<void()> Guarded();

        void Execute(const WriteRegister& step);
        void Execute(const WriteBurst& step);
        void Execute(const ReadBurst& step);
        void Execute(const ModifyRegister& step);
        void Execute(const Delay& step);
        void Execute(const Invoke& step);
        void Execute(const Await& step) const;

        RegisterBusAccess& bus;
        infra::AccessedBySharedPtr& sharedAccess;
        infra::TimerSingleShot delayTimer;
        infra::BoundedVector<Step>::WithMaxSize<maxSteps> steps;
        infra::AutoResetFunction<void()> onDone;
        std::size_t current = 0;
        uint8_t callbacksOutstanding = 0;
        uint8_t writeValue = 0;
        uint8_t modifyValue = 0;
        uint8_t modifyAddress = 0;
        uint8_t modifyClearMask = 0;
        uint8_t modifySetMask = 0;
        bool running = false;
    };
}

#endif
