#include "infra/util/ExecutionTimeStatistics.hpp"

namespace infra
{
    namespace detail
    {
        uint32_t SaturatingIncrement(uint32_t counter)
        {
            return counter == UINT32_MAX ? counter : counter + 1;
        }
    }

    void ExecutionTimeStatistics::Configure(uint32_t budget, uint32_t period)
    {
        budgetThreshold = budget == 0 ? disabled : budget;
        periodThreshold = period == 0 ? disabled : period;
        Reset();
    }

    void ExecutionTimeStatistics::Record(uint32_t duration)
    {
        last = duration;
        samples = detail::SaturatingIncrement(samples);

        if (duration < minimum)
            minimum = duration;
        if (duration > maximum)
            maximum = duration;

        scaledAverage = samples == 1
                            ? duration << averagingShift
                            : scaledAverage + duration - (scaledAverage >> averagingShift);

        if (duration > budgetThreshold)
            overruns = detail::SaturatingIncrement(overruns);
        if (duration > periodThreshold)
            deadlineMisses = detail::SaturatingIncrement(deadlineMisses);
    }

    void ExecutionTimeStatistics::RecordReentry()
    {
        reentries = detail::SaturatingIncrement(reentries);
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
