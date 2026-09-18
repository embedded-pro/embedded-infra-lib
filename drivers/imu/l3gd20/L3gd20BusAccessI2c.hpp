#ifndef DRIVERS_IMU_L3GD20_L3GD20_BUS_ACCESS_I2C_HPP
#define DRIVERS_IMU_L3GD20_L3GD20_BUS_ACCESS_I2C_HPP

#include "hal/interfaces/I2cRegisterAccess.hpp"
#include "services/util/RegisterBusAccess.hpp"

namespace drivers
{
    // The SDO pin selects between the two addresses. The device only auto-increments the sub-address
    // when bit 7 is set, so a burst that forgets it reads the same register over and over.
    class L3gd20BusAccessI2c
        : public services::RegisterBusAccess
    {
    public:
        static constexpr hal::I2cAddress addressSdoLow{ 0x6a };
        static constexpr hal::I2cAddress addressSdoHigh{ 0x6b };

        explicit L3gd20BusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address = addressSdoLow);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        static constexpr uint8_t autoIncrement = 0x80;

        static uint8_t SubAddress(uint8_t address, std::size_t size);

        hal::I2cMasterRegisterAccess<uint8_t> registerAccess;
    };
}

#endif
