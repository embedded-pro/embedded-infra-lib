#include "drivers/imu/mpu9250/Mpu9250BusAccessI2c.hpp"

namespace drivers
{
    Mpu9250BusAccessI2c::Mpu9250BusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address)
        : registerAccess(i2c, address)
    {}

    void Mpu9250BusAccessI2c::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.ReadRegister(address, data, onDone);
    }

    void Mpu9250BusAccessI2c::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.WriteRegister(address, data, onDone);
    }

    bool Mpu9250BusAccessI2c::RequiresI2cSlaveInterfaceDisabled() const
    {
        return false;
    }
}
