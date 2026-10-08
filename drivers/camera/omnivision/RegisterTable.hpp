#ifndef DRIVERS_CAMERA_OMNIVISION_REGISTER_TABLE_HPP
#define DRIVERS_CAMERA_OMNIVISION_REGISTER_TABLE_HPP

#include <cstdint>

namespace drivers
{
    struct RegisterStep
    {
        enum class Operation : uint8_t
        {
            write,
            modify,
            delay
        };

        Operation operation;
        uint8_t address;
        uint8_t value;
        uint8_t clearMask;

        static constexpr RegisterStep Write(uint8_t address, uint8_t value)
        {
            return { Operation::write, address, value, 0 };
        }

        static constexpr RegisterStep Modify(uint8_t address, uint8_t clearMask, uint8_t setMask)
        {
            return { Operation::modify, address, setMask, clearMask };
        }

        static constexpr RegisterStep DelayMilliseconds(uint8_t milliseconds)
        {
            return { Operation::delay, 0, milliseconds, 0 };
        }

        bool operator==(const RegisterStep&) const = default;
    };
}

#endif
