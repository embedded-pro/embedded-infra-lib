#ifndef HAL_ACCELEROMETER_MOCK_HPP
#define HAL_ACCELEROMETER_MOCK_HPP

#include "hal/interfaces/Accelerometer.hpp"
#include "gmock/gmock.h"

namespace hal
{
    template<class Unit, class Storage>
    class AccelerometerMock
        : public Accelerometer<Unit, Storage>
    {
    public:
        using Samples = typename Accelerometer<Unit, Storage>::Samples;

        MOCK_METHOD(void, Start, (const infra::Function<void(Samples)>& onMeasurement), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };
}

#endif // HAL_ACCELEROMETER_MOCK_HPP
