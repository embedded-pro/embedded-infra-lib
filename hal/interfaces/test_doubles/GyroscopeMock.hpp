#ifndef HAL_GYROSCOPE_MOCK_HPP
#define HAL_GYROSCOPE_MOCK_HPP

#include "hal/interfaces/Gyroscope.hpp"
#include "gmock/gmock.h"

namespace hal
{
    template<class Unit, class Storage>
    class GyroscopeMock
        : public Gyroscope<Unit, Storage>
    {
    public:
        using Samples = typename Gyroscope<Unit, Storage>::Samples;

        MOCK_METHOD(void, Start, (const infra::Function<void(Samples)>& onMeasurement), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };
}

#endif // HAL_GYROSCOPE_MOCK_HPP
