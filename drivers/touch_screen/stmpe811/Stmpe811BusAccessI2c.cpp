#include "drivers/touch_screen/stmpe811/Stmpe811BusAccessI2c.hpp"

namespace drivers
{
    Stmpe811BusAccessI2c::Stmpe811BusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address)
        : registerAccess(i2c, address)
    {}

    void Stmpe811BusAccessI2c::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.ReadRegister(address, data, onDone);
    }

    void Stmpe811BusAccessI2c::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.WriteRegister(address, data, onDone);
    }
}
