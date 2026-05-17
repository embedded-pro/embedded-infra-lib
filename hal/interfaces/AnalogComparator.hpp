#ifndef HAL_INTERFACES_ANALOG_COMPARATOR_HPP
#define HAL_INTERFACES_ANALOG_COMPARATOR_HPP

#include "hal/interfaces/Gpio.hpp"
#include "infra/util/Function.hpp"

namespace hal
{
    class AnalogComparator
    {
    protected:
        AnalogComparator() = default;
        AnalogComparator(const AnalogComparator&) = delete;
        AnalogComparator& operator=(const AnalogComparator&) = delete;
        ~AnalogComparator() = default;

    public:
        virtual void Enable(const infra::Function<void(bool output)>& onOutputChanged, InterruptTrigger trigger) = 0;
        virtual void Disable() = 0;
        virtual bool GetOutput() const = 0;
    };
}

#endif
