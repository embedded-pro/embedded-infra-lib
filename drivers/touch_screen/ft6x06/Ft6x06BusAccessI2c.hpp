#ifndef DRIVERS_TOUCH_SCREEN_FT6X06_FT6X06_BUS_ACCESS_I2C_HPP
#define DRIVERS_TOUCH_SCREEN_FT6X06_FT6X06_BUS_ACCESS_I2C_HPP

#include "hal/interfaces/I2cRegisterAccess.hpp"
#include "services/util/RegisterBusAccess.hpp"

namespace drivers
{
    class Ft6x06BusAccessI2c
        : public services::RegisterBusAccess
    {
    public:
        static constexpr hal::I2cAddress defaultAddress{ 0x38 };

        explicit Ft6x06BusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address = defaultAddress);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        hal::I2cMasterRegisterAccessByte registerAccess;
    };
}

#endif
