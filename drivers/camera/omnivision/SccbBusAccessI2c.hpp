#ifndef DRIVERS_CAMERA_OMNIVISION_SCCB_BUS_ACCESS_I2C_HPP
#define DRIVERS_CAMERA_OMNIVISION_SCCB_BUS_ACCESS_I2C_HPP

#include "hal/interfaces/I2c.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <cstdint>

namespace drivers
{
    class SccbBusAccessI2c
        : public services::RegisterBusAccess
    {
    public:
        SccbBusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address);
        SccbBusAccessI2c(const SccbBusAccessI2c&) = delete;
        SccbBusAccessI2c& operator=(const SccbBusAccessI2c&) = delete;

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        hal::I2cMaster& i2c;
        hal::I2cAddress deviceAddress;
        uint8_t registerByte{};
        infra::ByteRange readData;
        infra::ConstByteRange writeData;
        infra::Function<void()> onDone;
    };
}

#endif
