#ifndef HAL_MAGNETOMETER_MOCK_HPP
#define HAL_MAGNETOMETER_MOCK_HPP

#include "hal/interfaces/Magnetometer.hpp"
#include "gmock/gmock.h"

namespace hal
{
    template<class Unit, class Storage>
    class MagnetometerMock
        : public Magnetometer<Unit, Storage>
    {
    public:
        using Samples = typename Magnetometer<Unit, Storage>::Samples;

        MOCK_METHOD(void, Start, (const infra::Function<void(Samples)>& onMeasurement), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };
}

#endif // HAL_MAGNETOMETER_MOCK_HPP
