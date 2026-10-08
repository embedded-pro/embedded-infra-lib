#include "drivers/audio/cs43l22/Cs43l22BusAccessI2c.hpp"

namespace drivers
{
    Cs43l22BusAccessI2c::Cs43l22BusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address)
        : registerAccess(i2c, address)
    {}

    void Cs43l22BusAccessI2c::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.ReadRegister(MemoryAddressPointer(address, data.size()), data, onDone);
    }

    void Cs43l22BusAccessI2c::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        registerAccess.WriteRegister(MemoryAddressPointer(address, data.size()), data, onDone);
    }

    uint8_t Cs43l22BusAccessI2c::MemoryAddressPointer(uint8_t address, std::size_t size)
    {
        return size > 1 ? static_cast<uint8_t>(address | autoIncrement) : address;
    }
}
