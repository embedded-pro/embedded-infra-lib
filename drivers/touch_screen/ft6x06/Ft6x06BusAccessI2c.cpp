#include "drivers/touch_screen/ft6x06/Ft6x06BusAccessI2c.hpp"

namespace drivers
{
    Ft6x06BusAccessI2c::Ft6x06BusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address)
        : registerAccess(i2c, address)
    {}

    void Ft6x06BusAccessI2c::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.ReadRegister(address, data, onDone);
    }

    void Ft6x06BusAccessI2c::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.WriteRegister(address, data, onDone);
    }
}
