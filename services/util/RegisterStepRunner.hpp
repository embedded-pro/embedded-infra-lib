#ifndef SERVICES_REGISTER_STEP_RUNNER_HPP
#define SERVICES_REGISTER_STEP_RUNNER_HPP

#include "infra/event/EventDispatcher.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "infra/util/SharedPtr.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <cstdint>
#include <variant>

namespace services
{
    // Header-only on purpose: drivers include this without linking services.util, just like Stoppable.hpp
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

        struct Invoke
        {
            Action action;
        };

        struct Await
        {
            Action action;
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
        void Execute(const Await& step);

        RegisterBusAccess& bus;
        infra::AccessedBySharedPtr& sharedAccess;
        infra::TimerSingleShot delayTimer;
        infra::BoundedVector<Step>::WithMaxSize<maxSteps> steps;
        infra::AutoResetFunction<void()> onDone;
        std::size_t current = 0;
        uint8_t writeValue = 0;
        uint8_t modifyValue = 0;
        uint8_t modifyAddress = 0;
        uint8_t modifyClearMask = 0;
        uint8_t modifySetMask = 0;
        bool running = false;
    };

    ////    Implementation    ////

    inline RegisterStepRunner::RegisterStepRunner(RegisterBusAccess& bus, infra::AccessedBySharedPtr& sharedAccess)
        : bus(bus)
        , sharedAccess(sharedAccess)
    {}

    inline void RegisterStepRunner::Clear()
    {
        really_assert(!running);

        steps.clear();
        current = 0;
    }

    inline void RegisterStepRunner::Push(const Step& step)
    {
        really_assert(!running);
        really_assert(!steps.full());

        steps.push_back(step);
    }

    inline void RegisterStepRunner::Start(const infra::Function<void()>& onDone)
    {
        really_assert(!running);

        this->onDone = onDone;
        current = 0;
        running = true;

        ExecuteCurrentStep();
    }

    inline void RegisterStepRunner::Continue()
    {
        Advance();
    }

    inline void RegisterStepRunner::Abort()
    {
        running = false;
        onDone = nullptr;
        delayTimer.Cancel();
    }

    inline bool RegisterStepRunner::Busy() const
    {
        return running;
    }

    inline void RegisterStepRunner::Advance()
    {
        if (!running)
            return;

        ++current;
        ExecuteCurrentStep();
    }

    inline void RegisterStepRunner::ExecuteCurrentStep()
    {
        if (current == steps.size())
            return Complete();

        const Step& step = steps[current];

        if (auto write = std::get_if<WriteRegister>(&step); write != nullptr)
            Execute(*write);
        else if (auto writeBurst = std::get_if<WriteBurst>(&step); writeBurst != nullptr)
            Execute(*writeBurst);
        else if (auto read = std::get_if<ReadBurst>(&step); read != nullptr)
            Execute(*read);
        else if (auto modify = std::get_if<ModifyRegister>(&step); modify != nullptr)
            Execute(*modify);
        else if (auto delay = std::get_if<Delay>(&step); delay != nullptr)
            Execute(*delay);
        else if (auto invoke = std::get_if<Invoke>(&step); invoke != nullptr)
            Execute(*invoke);
        else
            Execute(*std::get_if<Await>(&step));
    }

    inline void RegisterStepRunner::Complete()
    {
        // Stays busy until the completion has been delivered, so a Start() from elsewhere cannot
        // overwrite onDone while this one is still queued
        infra::EventDispatcher::Instance().Schedule([self = sharedAccess.MakeShared(*this)]()
            {
                self->running = false;

                if (self->onDone)
                    self->onDone();
            });
    }

    inline infra::Function<void()> RegisterStepRunner::Guarded()
    {
        return [self = sharedAccess.MakeShared(*this)]()
        {
            self->Advance();
        };
    }

    inline void RegisterStepRunner::Execute(const WriteRegister& step)
    {
        writeValue = step.value;
        bus.WriteRegister(step.address, infra::MakeByteRange(writeValue), Guarded());
    }

    inline void RegisterStepRunner::Execute(const WriteBurst& step)
    {
        bus.WriteRegister(step.address, step.data, Guarded());
    }

    inline void RegisterStepRunner::Execute(const ReadBurst& step)
    {
        bus.ReadRegister(step.address, step.data, Guarded());
    }

    inline void RegisterStepRunner::Execute(const ModifyRegister& step)
    {
        modifyAddress = step.address;
        modifyClearMask = step.clearMask;
        modifySetMask = step.setMask;

        bus.ReadRegister(modifyAddress, infra::MakeByteRange(modifyValue), [self = sharedAccess.MakeShared(*this)]()
            {
                self->writeValue = static_cast<uint8_t>((self->modifyValue & ~self->modifyClearMask) | self->modifySetMask);
                self->bus.WriteRegister(self->modifyAddress, infra::MakeByteRange(self->writeValue), self->Guarded());
            });
    }

    inline void RegisterStepRunner::Execute(const Delay& step)
    {
        delayTimer.Start(step.duration, Guarded());
    }

    inline void RegisterStepRunner::Execute(const Invoke& step)
    {
        step.action();
        Advance();
    }

    inline void RegisterStepRunner::Execute(const Await& step)
    {
        step.action();
    }
}

#endif
