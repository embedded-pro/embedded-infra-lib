#include "drivers/camera/omnivision/RegisterTableRunner.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <chrono>

namespace drivers
{
    RegisterTableRunner::RegisterTableRunner(services::RegisterBusAccess& bus)
        : bus(bus)
    {}

    RegisterTableRunner::~RegisterTableRunner()
    {
        really_assert(!busy);
    }

    void RegisterTableRunner::Run(infra::MemoryRange<const RegisterStep> steps, const infra::Function<void()>& onDone)
    {
        really_assert(!busy);

        this->steps = steps;
        this->onDone = onDone;
        this->stepIndex = 0;
        busy = true;

        if (steps.empty())
        {
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    Complete();
                });
        }
        else
            ExecuteCurrentStep();
    }

    bool RegisterTableRunner::Busy() const
    {
        return busy;
    }

    void RegisterTableRunner::ExecuteCurrentStep()
    {
        const RegisterStep& step = steps[stepIndex];

        switch (step.operation)
        {
            case RegisterStep::Operation::write:
                WriteCurrentStep();
                break;
            case RegisterStep::Operation::modify:
                bus.ReadRegister(step.address, infra::MakeByteRange(readBuffer[0]),
                    [this]()
                    {
                        ModifyReadDone();
                    });
                break;
            case RegisterStep::Operation::delay:
                timer.Start(std::chrono::milliseconds(step.value),
                    [this]()
                    {
                        NextStep();
                    });
                break;
        }
    }

    void RegisterTableRunner::WriteCurrentStep()
    {
        valueBuffer[0] = steps[stepIndex].value;
        bus.WriteRegister(steps[stepIndex].address, infra::MakeByteRange(valueBuffer[0]),
            [this]()
            {
                NextStep();
            });
    }

    void RegisterTableRunner::ModifyReadDone()
    {
        const RegisterStep& step = steps[stepIndex];
        valueBuffer[0] = static_cast<uint8_t>((readBuffer[0] & ~step.clearMask) | step.value);
        bus.WriteRegister(step.address, infra::MakeByteRange(valueBuffer[0]),
            [this]()
            {
                ModifyWriteDone();
            });
    }

    void RegisterTableRunner::ModifyWriteDone()
    {
        NextStep();
    }

    void RegisterTableRunner::NextStep()
    {
        ++stepIndex;

        if (stepIndex == steps.size())
            Complete();
        else
            ExecuteCurrentStep();
    }

    void RegisterTableRunner::Complete()
    {
        busy = false;
        infra::Function<void()> done = onDone;
        onDone = nullptr;
        done();
    }
}
