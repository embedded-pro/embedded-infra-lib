#ifndef DRIVERS_IMU_MPU9250_MPU9250_BUS_ACCESS_I2C_HPP
#define DRIVERS_IMU_MPU9250_MPU9250_BUS_ACCESS_I2C_HPP

#include "drivers/imu/mpu9250/Mpu9250BusAccess.hpp"
#include "hal/interfaces/I2cRegisterAccess.hpp"

namespace drivers
{
    class Mpu9250BusAccessI2c
        : public Mpu9250BusAccess
    {
    public:
        static constexpr hal::I2cAddress addressAd0Low{ 0x68 };
        static constexpr hal::I2cAddress addressAd0High{ 0x69 };

        explicit Mpu9250BusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address = addressAd0Low);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;
        bool RequiresI2cSlaveInterfaceDisabled() const override;

    private:
        hal::I2cMasterRegisterAccess<uint8_t> registerAccess;
    };
}

#endif
