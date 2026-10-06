#include "drivers/audio/wm8994/Wm8994BusAccessI2c.hpp"

namespace drivers
{
    Wm8994BusAccessI2c::Wm8994BusAccessI2c(hal::I2cMaster& i2c)
        : registerAccess(i2c, deviceAddress)
    {}

    void Wm8994BusAccessI2c::ReadRegister(uint16_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.ReadRegister(address, data, onDone);
    }

    void Wm8994BusAccessI2c::WriteRegister(uint16_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.WriteRegister(address, data, onDone);
    }
}
