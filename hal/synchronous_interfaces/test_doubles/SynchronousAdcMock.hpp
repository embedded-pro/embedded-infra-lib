#ifndef HAL_SYNCHRONOUS_ADC_MOCK_HPP
#define HAL_SYNCHRONOUS_ADC_MOCK_HPP

#include "hal/synchronous_interfaces/SynchronousAdc.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class SynchronousAdcMock
        : public SynchronousAdc
    {
    public:
        MOCK_METHOD(Samples, Measure, (std::size_t numberOfSamples), (override));
    };
}

#endif
