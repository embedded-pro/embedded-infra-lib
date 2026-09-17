#include "drivers/imu/lsm303dlhc/Lsm303dlhcBusAccessI2c.hpp"

namespace drivers
{
    Lsm303dlhcAccelerometerBusAccessI2c::Lsm303dlhcAccelerometerBusAccessI2c(hal::I2cMaster& i2c)
        : registerAccess(i2c, address)
    {}

    void Lsm303dlhcAccelerometerBusAccessI2c::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.ReadRegister(SubAddress(address, data.size()), data, onDone);
    }

    void Lsm303dlhcAccelerometerBusAccessI2c::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.WriteRegister(SubAddress(address, data.size()), data, onDone);
    }

    uint8_t Lsm303dlhcAccelerometerBusAccessI2c::SubAddress(uint8_t address, std::size_t size)
    {
        return size > 1 ? static_cast<uint8_t>(address | autoIncrement) : address;
    }

    Lsm303dlhcMagnetometerBusAccessI2c::Lsm303dlhcMagnetometerBusAccessI2c(hal::I2cMaster& i2c)
        : registerAccess(i2c, address)
    {}

    void Lsm303dlhcMagnetometerBusAccessI2c::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.ReadRegister(address, data, onDone);
    }

    void Lsm303dlhcMagnetometerBusAccessI2c::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.WriteRegister(address, data, onDone);
    }
}
