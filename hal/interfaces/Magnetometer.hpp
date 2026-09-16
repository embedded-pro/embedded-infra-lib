#ifndef HAL_MAGNETOMETER_HPP
#define HAL_MAGNETOMETER_HPP

#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/Unit.hpp"

namespace hal
{
    template<class Unit, class Storage>
    class Magnetometer
    {
    public:
        using Samples = infra::MemoryRange<const infra::Quantity<Unit, Storage>>;

        virtual void Start(const infra::Function<void(Samples)>& onMeasurement) = 0;
        virtual void Stop() = 0;

    protected:
        Magnetometer() = default;
        Magnetometer(const Magnetometer& other) = delete;
        Magnetometer& operator=(const Magnetometer& other) = delete;
        ~Magnetometer() = default;
    };
}

#endif // HAL_MAGNETOMETER_HPP
