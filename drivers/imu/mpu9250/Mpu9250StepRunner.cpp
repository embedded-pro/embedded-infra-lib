#include "drivers/imu/mpu9250/Mpu9250StepRunner.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    Mpu9250StepRunner::Mpu9250StepRunner(Mpu9250BusAccess& bus, infra::AccessedBySharedPtr& sharedAccess)
        : bus(bus)
        , sharedAccess(sharedAccess)
    {}

    void Mpu9250StepRunner::Clear()
    {
        really_assert(!running);

        steps.clear();
        current = 0;
    }

    void Mpu9250StepRunner::Push(const Step& step)
    {
        really_assert(!running);
        really_assert(!steps.full());

        steps.push_back(step);
    }

    void Mpu9250StepRunner::Start(const infra::Function<void()>& onDone)
    {
        really_assert(!running);

        this->onDone = onDone;
        current = 0;
        running = true;

        ExecuteCurrentStep();
    }

    void Mpu9250StepRunner::Continue()
    {
        Advance();
    }

    void Mpu9250StepRunner::Abort()
    {
        running = false;
        onDone = nullptr;
        delayTimer.Cancel();
    }

    bool Mpu9250StepRunner::Busy() const
    {
        return running;
    }

    void Mpu9250StepRunner::Advance()
    {
        if (!running)
            return;

        ++current;
        ExecuteCurrentStep();
    }

    void Mpu9250StepRunner::ExecuteCurrentStep()
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

    void Mpu9250StepRunner::Complete()
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

    infra::Function<void()> Mpu9250StepRunner::Guarded()
    {
        return [self = sharedAccess.MakeShared(*this)]()
        {
            self->Advance();
        };
    }

    void Mpu9250StepRunner::Execute(const WriteRegister& step)
    {
        writeValue = step.value;
        bus.WriteRegister(step.address, infra::MakeByteRange(writeValue), Guarded());
    }

    void Mpu9250StepRunner::Execute(const WriteBurst& step)
    {
        bus.WriteRegister(step.address, step.data, Guarded());
    }

    void Mpu9250StepRunner::Execute(const ReadBurst& step)
    {
        bus.ReadRegister(step.address, step.data, Guarded());
    }

    void Mpu9250StepRunner::Execute(const ModifyRegister& step)
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

    void Mpu9250StepRunner::Execute(const Delay& step)
    {
        delayTimer.Start(step.duration, Guarded());
    }

    void Mpu9250StepRunner::Execute(const Invoke& step)
    {
        step.action();
        Advance();
    }

    void Mpu9250StepRunner::Execute(const Await& step)
    {
        step.action();
    }
}
