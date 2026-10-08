#ifndef DRIVERS_IMU_COMMON_IMU_BUS_ACCESS_I2C_HPP
#define DRIVERS_IMU_COMMON_IMU_BUS_ACCESS_I2C_HPP

#include "hal/interfaces/I2cRegisterAccess.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <cstdint>

namespace drivers
{
    // autoIncrementFlag is ORed into the sub-address when size > 1 to enable burst reads/writes.
    class ImuBusAccessI2c
        : public services::RegisterBusAccess
    {
    public:
        explicit ImuBusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address, uint8_t autoIncrementFlag = 0);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        uint8_t SubAddress(uint8_t address, std::size_t size) const;

        hal::I2cMasterRegisterAccess<uint8_t> registerAccess;
        uint8_t autoIncrementFlag;
    };
}

#endif
