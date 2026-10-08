#ifndef DRIVERS_TOUCH_SCREEN_STMPE811_STMPE811_BUS_ACCESS_I2C_HPP
#define DRIVERS_TOUCH_SCREEN_STMPE811_STMPE811_BUS_ACCESS_I2C_HPP

#include "hal/interfaces/I2cRegisterAccess.hpp"
#include "services/util/RegisterBusAccess.hpp"

namespace drivers
{
    // The ADDR0 pin selects between the two addresses
    class Stmpe811BusAccessI2c
        : public services::RegisterBusAccess
    {
    public:
        static constexpr hal::I2cAddress addressAddr0Low{ 0x41 };
        static constexpr hal::I2cAddress addressAddr0High{ 0x44 };

        explicit Stmpe811BusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address = addressAddr0Low);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        hal::I2cMasterRegisterAccessByte registerAccess;
    };
}

#endif
