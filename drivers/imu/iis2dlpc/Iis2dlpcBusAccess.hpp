#ifndef DRIVERS_IMU_IIS2DLPC_IIS2DLPC_BUS_ACCESS_HPP
#define DRIVERS_IMU_IIS2DLPC_IIS2DLPC_BUS_ACCESS_HPP

#include "drivers/imu/common/ImuBusAccessI2c.hpp"
#include "drivers/imu/common/ImuBusAccessSpi.hpp"
#include "hal/interfaces/I2c.hpp"
#include <cstdint>

namespace drivers
{
    class Iis2dlpcBusAccessSpi
        : public ImuBusAccessSpi
    {
    public:
        explicit Iis2dlpcBusAccessSpi(hal::SpiMaster& spiWithChipSelect)
            : ImuBusAccessSpi(spiWithChipSelect, 0)
        {}
    };

    class Iis2dlpcBusAccessI2c
        : public ImuBusAccessI2c
    {
    public:
        static constexpr hal::I2cAddress addressSa0Low{ 0x18 };
        static constexpr hal::I2cAddress addressSa0High{ 0x19 };

        explicit Iis2dlpcBusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address = addressSa0Low)
            : ImuBusAccessI2c(i2c, address, 0)
        {}
    };
}

#endif
