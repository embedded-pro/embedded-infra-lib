#ifndef HAL_DUTY_CYCLE_HPP
#define HAL_DUTY_CYCLE_HPP

#include <cstdint>

namespace hal
{
    // A PWM duty cycle in Q16: fullScale is 100 %. Integer so that the conversion to timer counts
    // needs a multiply and a shift, never a division, in the interrupts that update it.
    class DutyCycle
    {
    public:
        static constexpr uint32_t fractionalBits = 16;
        static constexpr uint32_t fullScale = uint32_t{ 1 } << fractionalBits;

        constexpr DutyCycle() = default;

        constexpr explicit DutyCycle(uint32_t value)
            : value(value)
        {}

        static constexpr DutyCycle FromPercent(uint32_t percent)
        {
            return DutyCycle((uint64_t{ percent } * fullScale + 50) / 100);
        }

        constexpr uint32_t Value() const
        {
            return value;
        }

        constexpr bool IsValid() const
        {
            return value <= fullScale;
        }

        // Rounds to the nearest count of a timer period of the given length; 64-bit so that a 32-bit
        // timer's full 2^32-count period fits
        constexpr uint64_t ToCounts(uint64_t periodCounts) const
        {
            return (periodCounts * value + fullScale / 2) >> fractionalBits;
        }

        constexpr bool operator==(const DutyCycle& other) const = default;

    private:
        uint32_t value{ 0 };
    };
}

#endif
