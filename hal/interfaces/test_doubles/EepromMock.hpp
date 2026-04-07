#ifndef HAL_STUB_EEPROM_MOCK_HPP
#define HAL_STUB_EEPROM_MOCK_HPP

#include "hal/interfaces/Eeprom.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class EepromMock
        : public hal::Eeprom
    {
    public:
        explicit EepromMock(uint32_t size);

        uint32_t Size() const override;
        void WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;
        void Erase(infra::Function<void()> onDone) override;

        uint32_t size;

        MOCK_METHOD2(writeBufferMock, void(std::vector<uint8_t>, uint32_t));
        MOCK_METHOD1(readBufferMock, std::vector<uint8_t>(uint32_t));
        MOCK_METHOD0(eraseMock, void());

        infra::AutoResetFunction<void()> done;
    };

    class CleanEepromMock
        : public hal::Eeprom
    {
    public:
        CleanEepromMock() = default;
        explicit CleanEepromMock(uint32_t size);

        MOCK_CONST_METHOD0(Size, uint32_t());
        MOCK_METHOD3(WriteBuffer, void(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone));
        MOCK_METHOD3(ReadBuffer, void(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone));
        MOCK_METHOD1(Erase, void(infra::Function<void()> onDone));
    };
}

#endif
