#ifndef HAL_INTERFACE_EEPROM_HPP
#define HAL_INTERFACE_EEPROM_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace hal
{
    class Eeprom
    {
    public:
        virtual uint32_t Size() const = 0;
        virtual void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) = 0;
        virtual void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) = 0;
        virtual void Erase(infra::Function<void()> onDone) = 0;
    };
}

#endif
