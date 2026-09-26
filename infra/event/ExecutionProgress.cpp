#include "infra/event/ExecutionProgress.hpp"

namespace infra
{
    ExecutionProgress::ActionScope::ActionScope(ExecutionProgress& progress)
        : progress(progress)
    {
        progress.Step();
    }

    ExecutionProgress::ActionScope::~ActionScope()
    {
        progress.Step();
    }

    uint32_t ExecutionProgress::Steps() const
    {
        return steps;
    }

    bool ExecutionProgress::IsExecuting(uint32_t steps)
    {
        return steps % 2 != 0;
    }

    void ExecutionProgress::Step()
    {
        steps.store(steps.load() + 1);
    }
}
