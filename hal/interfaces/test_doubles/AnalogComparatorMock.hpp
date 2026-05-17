#ifndef HAL_INTERFACES_TEST_DOUBLES_ANALOG_COMPARATOR_MOCK_HPP
#define HAL_INTERFACES_TEST_DOUBLES_ANALOG_COMPARATOR_MOCK_HPP

#include "hal/interfaces/AnalogComparator.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class AnalogComparatorMock
        : public AnalogComparator
    {
    public:
        MOCK_METHOD(void, Enable, (const infra::Function<void(bool output)>& onOutputChanged, InterruptTrigger trigger), (override));
        MOCK_METHOD(void, Disable, (), (override));
        MOCK_METHOD(bool, GetOutput, (), (const, override));
    };
}

#endif
