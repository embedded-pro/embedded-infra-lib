#ifndef HAL_SYNCHRONOUS_INTERFACES_SYNCHRONOUS_ANALOG_COMPARATOR_HPP
#define HAL_SYNCHRONOUS_INTERFACES_SYNCHRONOUS_ANALOG_COMPARATOR_HPP

namespace hal
{
    class SynchronousAnalogComparator
    {
    protected:
        SynchronousAnalogComparator() = default;
        SynchronousAnalogComparator(const SynchronousAnalogComparator&) = delete;
        SynchronousAnalogComparator& operator=(const SynchronousAnalogComparator&) = delete;
        ~SynchronousAnalogComparator() = default;

    public:
        virtual bool GetOutput() const = 0;
    };
}

#endif
