#ifndef DRIVERS_IMU_LIS3DSH_LIS3DSH_BUS_ACCESS_HPP
#define DRIVERS_IMU_LIS3DSH_LIS3DSH_BUS_ACCESS_HPP

#include "drivers/imu/common/ImuBusAccessI2c.hpp"
#include "drivers/imu/common/ImuBusAccessSpi.hpp"
#include "hal/interfaces/I2c.hpp"
#include <cstdint>

namespace drivers
{
    // LIS3DSH SPI framing: read bit (0x80) only.
    // Address auto-increment is enabled in CTRL_REG6.ADD_INC during initialisation,
    // not via a per-transaction flag in the address byte.
    class Lis3dshBusAccessSpi
        : public ImuBusAccessSpi
    {
    public:
        explicit Lis3dshBusAccessSpi(hal::SpiMaster& spiWithChipSelect)
            : ImuBusAccessSpi(spiWithChipSelect, 0)
        {}
    };

    // LIS3DSH I2C framing: plain sub-address with no auto-increment flag.
    // Increment is enabled via CTRL_REG6.ADD_INC during initialisation.
    class Lis3dshBusAccessI2c
        : public ImuBusAccessI2c
    {
    public:
        static constexpr hal::I2cAddress addressSelLow{ 0x1e };
        static constexpr hal::I2cAddress addressSelHigh{ 0x1d };

        explicit Lis3dshBusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address = addressSelLow)
            : ImuBusAccessI2c(i2c, address, 0)
        {}
    };
}

#endif
