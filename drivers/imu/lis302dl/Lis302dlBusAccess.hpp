#ifndef DRIVERS_IMU_LIS302DL_LIS302DL_BUS_ACCESS_HPP
#define DRIVERS_IMU_LIS302DL_LIS302DL_BUS_ACCESS_HPP

#include "drivers/imu/common/ImuBusAccessI2c.hpp"
#include "drivers/imu/common/ImuBusAccessSpi.hpp"
#include "hal/interfaces/I2c.hpp"
#include <cstdint>

namespace drivers
{
    class Lis302dlBusAccessSpi
        : public ImuBusAccessSpi
    {
    public:
        static constexpr uint8_t multipleByteFlag = 0x40;

        explicit Lis302dlBusAccessSpi(hal::SpiMaster& spiWithChipSelect)
            : ImuBusAccessSpi(spiWithChipSelect, multipleByteFlag)
        {}
    };

    class Lis302dlBusAccessI2c
        : public ImuBusAccessI2c
    {
    public:
        static constexpr hal::I2cAddress addressSdoLow{ 0x1c };
        static constexpr hal::I2cAddress addressSdoHigh{ 0x1d };
        static constexpr uint8_t autoIncrementFlag = 0x80;

        explicit Lis302dlBusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address = addressSdoLow)
            : ImuBusAccessI2c(i2c, address, autoIncrementFlag)
        {}
    };
}

#endif
