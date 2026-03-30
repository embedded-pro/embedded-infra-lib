#ifndef HAL_SYNCHRONOUS_ADC_HPP
#define HAL_SYNCHRONOUS_ADC_HPP

#include "hal/synchronous_interfaces/SynchronousQuadSpi.hpp"
#include "infra/util/MemoryRange.hpp"
#include <cstdint>

namespace hal
{
    class SynchronousAdc
    {
    public:
        using Samples = infra::MemoryRange<const uint16_t>;

        virtual Samples Measure(std::size_t numberOfSamples) = 0;
    };
}

#endif
