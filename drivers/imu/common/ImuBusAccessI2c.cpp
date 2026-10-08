#include "drivers/imu/common/ImuBusAccessI2c.hpp"

namespace drivers
{
    ImuBusAccessI2c::ImuBusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address, uint8_t autoIncrementFlag)
        : registerAccess(i2c, address)
        , autoIncrementFlag(autoIncrementFlag)
    {}

    void ImuBusAccessI2c::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.ReadRegister(SubAddress(address, data.size()), data, onDone);
    }

    void ImuBusAccessI2c::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.WriteRegister(SubAddress(address, data.size()), data, onDone);
    }

    uint8_t ImuBusAccessI2c::SubAddress(uint8_t address, std::size_t size) const
    {
        return size > 1 ? static_cast<uint8_t>(address | autoIncrementFlag) : address;
    }
}
