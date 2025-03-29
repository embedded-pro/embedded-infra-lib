#ifndef HAL_SYNCHRONOUS_ADC_HPP
#define HAL_SYNCHRONOUS_ADC_HPP

#include "infra/util/MemoryRange.hpp"
#include <cstdint>

namespace hal
{
    class AdcMultiChannel
    {
    public:
        using Samples = infra::MemoryRange<const uint16_t>;

        virtual Samples Measure(std::size_t numberOfSamples) = 0;
    };
}

#endif
