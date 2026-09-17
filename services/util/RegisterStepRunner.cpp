#include "services/util/RegisterStepRunner.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    RegisterStepRunner::RegisterStepRunner(RegisterBusAccess& bus, infra::AccessedBySharedPtr& sharedAccess)
        : bus(bus)
        , sharedAccess(sharedAccess)
    {}

    void RegisterStepRunner::Clear()
    {
        really_assert(!running);

        steps.clear();
        current = 0;
    }

    void RegisterStepRunner::Push(const Step& step)
    {
        really_assert(!running);
        really_assert(!steps.full());

        steps.push_back(step);
    }

    void RegisterStepRunner::Start(const infra::Function<void()>& onDone)
    {
        really_assert(!Busy());

        this->onDone = onDone;
        current = 0;
        running = true;

        ExecuteCurrentStep();
    }

    void RegisterStepRunner::Continue()
    {
        Advance();
    }

    void RegisterStepRunner::Abort()
    {
        running = false;
        onDone = nullptr;
        delayTimer.Cancel();
    }

    bool RegisterStepRunner::Busy() const
    {
        // A run that has been aborted stays busy until the callback it already issued has been
        // delivered, so a next run cannot be driven by the previous run's callback
        return running || callbacksOutstanding != 0;
    }

    void RegisterStepRunner::Advance()
    {
        if (!running)
            return;

        ++current;
        ExecuteCurrentStep();
    }

    void RegisterStepRunner::ExecuteCurrentStep()
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

    void RegisterStepRunner::Complete()
    {
        // Stays busy until the completion has been delivered, so a Start() from elsewhere cannot
        // overwrite onDone while this one is still queued
        ++callbacksOutstanding;

        infra::EventDispatcher::Instance().Schedule([self = sharedAccess.MakeShared(*this)]()
            {
                --self->callbacksOutstanding;
                self->running = false;

                if (self->onDone)
                    self->onDone();
            });
    }

    infra::Function<void()> RegisterStepRunner::Guarded()
    {
        ++callbacksOutstanding;

        return [self = sharedAccess.MakeShared(*this)]()
        {
            --self->callbacksOutstanding;
            self->Advance();
        };
    }

    void RegisterStepRunner::Execute(const WriteRegister& step)
    {
        writeValue = step.value;
        bus.WriteRegister(step.address, infra::MakeByteRange(writeValue), Guarded());
    }

    void RegisterStepRunner::Execute(const WriteBurst& step)
    {
        bus.WriteRegister(step.address, step.data, Guarded());
    }

    void RegisterStepRunner::Execute(const ReadBurst& step)
    {
        bus.ReadRegister(step.address, step.data, Guarded());
    }

    void RegisterStepRunner::Execute(const ModifyRegister& step)
    {
        modifyAddress = step.address;
        modifyClearMask = step.clearMask;
        modifySetMask = step.setMask;

        ++callbacksOutstanding;

        bus.ReadRegister(modifyAddress, infra::MakeByteRange(modifyValue), [self = sharedAccess.MakeShared(*this)]()
            {
                --self->callbacksOutstanding;

                if (!self->running)
                    return;

                self->writeValue = static_cast<uint8_t>((self->modifyValue & ~self->modifyClearMask) | self->modifySetMask);
                self->bus.WriteRegister(self->modifyAddress, infra::MakeByteRange(self->writeValue), self->Guarded());
            });
    }

    void RegisterStepRunner::Execute(const Delay& step)
    {
        delayTimer.Start(step.duration, Guarded());
    }

    void RegisterStepRunner::Execute(const Invoke& step)
    {
        step.action();
        Advance();
    }

    void RegisterStepRunner::Execute(const Await& step) const
    {
        step.action();
    }
}
