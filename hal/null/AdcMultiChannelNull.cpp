#include "hal/null/AdcMultiChannelNull.hpp"
#include <algorithm>

namespace hal
{
    AdcMultiChannelNull::AdcMultiChannelNull(infra::MemoryRange<uint16_t> samples, uint16_t neutralCode)
        : samples{ samples }
    {
        std::fill(samples.begin(), samples.end(), neutralCode);
    }

    void AdcMultiChannelNull::Measure(const infra::Function<void(Samples)>& onDone)
    {
        onDone(samples);
    }

    void AdcMultiChannelNull::Stop()
    {}
}
