#ifndef HAL_GYROSCOPE_HPP
#define HAL_GYROSCOPE_HPP

#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/Unit.hpp"

namespace hal
{
    template<class Unit, class Storage>
    class Gyroscope
    {
    public:
        using Samples = infra::MemoryRange<const infra::Quantity<Unit, Storage>>;

        virtual void Start(const infra::Function<void(Samples)>& onMeasurement) = 0;
        virtual void Stop() = 0;

    protected:
        Gyroscope() = default;
        Gyroscope(const Gyroscope& other) = delete;
        Gyroscope& operator=(const Gyroscope& other) = delete;
        ~Gyroscope() = default;
    };
}

#endif // HAL_GYROSCOPE_HPP
