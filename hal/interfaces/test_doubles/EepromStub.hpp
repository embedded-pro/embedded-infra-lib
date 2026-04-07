#ifndef HAL_STUB_EEPROM_STUB_HPP
#define HAL_STUB_EEPROM_STUB_HPP

#include "hal/interfaces/Eeprom.hpp"
#include <vector>

namespace hal
{
    class EepromStub
        : public hal::Eeprom
    {
    public:
        explicit EepromStub(uint32_t size);

        uint32_t Size() const override;
        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;

    private:
        std::vector<uint8_t> storage;
    };
}

#endif
