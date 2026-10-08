#ifndef DRIVERS_AUDIO_CS43L22_CS43L22_BUS_ACCESS_I2C_HPP
#define DRIVERS_AUDIO_CS43L22_CS43L22_BUS_ACCESS_I2C_HPP

#include "hal/interfaces/I2cRegisterAccess.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <cstddef>
#include <cstdint>

namespace drivers
{
    class Cs43l22BusAccessI2c
        : public services::RegisterBusAccess
    {
    public:
        static constexpr hal::I2cAddress addressAd0Low{ 0x4a };
        static constexpr hal::I2cAddress addressAd0High{ 0x4b };

        explicit Cs43l22BusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address = addressAd0Low);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        static constexpr uint8_t autoIncrement = 0x80;

        static uint8_t MemoryAddressPointer(uint8_t address, std::size_t size);

        hal::I2cMasterRegisterAccess<uint8_t> registerAccess;
    };
}

#endif
