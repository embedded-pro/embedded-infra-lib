#ifndef HAL_SYNCHRONOUS_INTERFACES_TEST_DOUBLES_SYNCHRONOUS_ANALOG_COMPARATOR_MOCK_HPP
#define HAL_SYNCHRONOUS_INTERFACES_TEST_DOUBLES_SYNCHRONOUS_ANALOG_COMPARATOR_MOCK_HPP

#include "hal/synchronous_interfaces/SynchronousAnalogComparator.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class SynchronousAnalogComparatorMock
        : public SynchronousAnalogComparator
    {
    public:
        MOCK_METHOD(bool, GetOutput, (), (const, override));
    };
}

#endif
