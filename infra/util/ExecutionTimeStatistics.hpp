#ifndef INFRA_EXECUTION_TIME_STATISTICS_HPP
#define INFRA_EXECUTION_TIME_STATISTICS_HPP

#include <cstdint>

namespace infra
{
    namespace detail
    {
        constexpr uint32_t SaturatingIncrement(uint32_t counter)
        {
            return counter == UINT32_MAX ? counter : counter + 1;
        }
    }

    class ExecutionTimeStatistics
    {
    public:
        struct Snapshot
        {
            uint32_t samples{ 0 };
            uint32_t last{ 0 };
            uint32_t minimum{ 0 };
            uint32_t maximum{ 0 };
            uint32_t average{ 0 };
            uint32_t budget{ 0 };
            uint32_t overruns{ 0 };
            uint32_t deadlineMisses{ 0 };
            uint32_t reentries{ 0 };
        };

        void Configure(uint32_t budget, uint32_t period);

        void Record(uint32_t duration)
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

        void RecordReentry()
        {
            reentries = detail::SaturatingIncrement(reentries);
        }

        void Reset();

        Snapshot Read() const;

    private:
        static constexpr uint32_t disabled = UINT32_MAX;
        static constexpr uint32_t averagingShift = 4;

        uint32_t budgetThreshold{ disabled };
        uint32_t periodThreshold{ disabled };
        uint32_t samples{ 0 };
        uint32_t last{ 0 };
        uint32_t minimum{ UINT32_MAX };
        uint32_t maximum{ 0 };
        uint32_t scaledAverage{ 0 };
        uint32_t overruns{ 0 };
        uint32_t deadlineMisses{ 0 };
        uint32_t reentries{ 0 };
    };
}

#endif
