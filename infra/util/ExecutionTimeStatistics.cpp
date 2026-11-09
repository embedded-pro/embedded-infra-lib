#include "infra/util/ExecutionTimeStatistics.hpp"

namespace infra
{
    void ExecutionTimeStatistics::Configure(uint32_t budget, uint32_t period)
    {
        budgetThreshold = budget == 0 ? disabled : budget;
        periodThreshold = period == 0 ? disabled : period;
        Reset();
    }

    void ExecutionTimeStatistics::Reset()
    {
        samples = 0;
        last = 0;
        minimum = UINT32_MAX;
        maximum = 0;
        scaledAverage = 0;
        overruns = 0;
        deadlineMisses = 0;
        reentries = 0;
    }

    ExecutionTimeStatistics::Snapshot ExecutionTimeStatistics::Read() const
    {
        return Snapshot{
            samples,
            last,
            samples == 0 ? 0u : minimum,
            maximum,
            scaledAverage >> averagingShift,
            budgetThreshold == disabled ? 0u : budgetThreshold,
            overruns,
            deadlineMisses,
            reentries
        };
    }
}
