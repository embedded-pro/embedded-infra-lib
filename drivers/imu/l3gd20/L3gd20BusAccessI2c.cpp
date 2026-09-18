#include "drivers/imu/l3gd20/L3gd20BusAccessI2c.hpp"

namespace drivers
{
    L3gd20BusAccessI2c::L3gd20BusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address)
        : registerAccess(i2c, address)
    {}

    void L3gd20BusAccessI2c::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.ReadRegister(SubAddress(address, data.size()), data, onDone);
    }

    void L3gd20BusAccessI2c::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.WriteRegister(SubAddress(address, data.size()), data, onDone);
    }

    uint8_t L3gd20BusAccessI2c::SubAddress(uint8_t address, std::size_t size)
    {
        return size > 1 ? static_cast<uint8_t>(address | autoIncrement) : address;
    }
}
