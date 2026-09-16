#ifndef HAL_ACCELEROMETER_HPP
#define HAL_ACCELEROMETER_HPP

#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/Unit.hpp"

namespace hal
{
    template<class Unit, class Storage>
    class Accelerometer
    {
    public:
        using Samples = infra::MemoryRange<const infra::Quantity<Unit, Storage>>;

        virtual void Start(const infra::Function<void(Samples)>& onMeasurement) = 0;
        virtual void Stop() = 0;

    protected:
        Accelerometer() = default;
        Accelerometer(const Accelerometer& other) = delete;
        Accelerometer& operator=(const Accelerometer& other) = delete;
        ~Accelerometer() = default;
    };
}

#endif // HAL_ACCELEROMETER_HPP
