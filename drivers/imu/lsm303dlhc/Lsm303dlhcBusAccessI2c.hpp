#ifndef DRIVERS_IMU_LSM303DLHC_LSM303DLHC_BUS_ACCESS_I2C_HPP
#define DRIVERS_IMU_LSM303DLHC_LSM303DLHC_BUS_ACCESS_I2C_HPP

#include "hal/interfaces/I2cRegisterAccess.hpp"
#include "services/util/RegisterBusAccess.hpp"

namespace drivers
{
    // The LSM303DLHC presents two independent slaves on one bus. When both are driven from the same
    // hal::I2cMaster, interpose a services::I2cMultipleAccess so a repeated-start sequence of one
    // block cannot be interrupted by the other.

    // The linear acceleration block only auto-increments the sub-address when bit 7 is set
    class Lsm303dlhcAccelerometerBusAccessI2c
        : public services::RegisterBusAccess
    {
    public:
        static constexpr hal::I2cAddress address{ 0x19 };

        explicit Lsm303dlhcAccelerometerBusAccessI2c(hal::I2cMaster& i2c);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        static constexpr uint8_t autoIncrement = 0x80;

        static uint8_t SubAddress(uint8_t address, std::size_t size);

        hal::I2cMasterRegisterAccess<uint8_t> registerAccess;
    };

    // The magnetic block always auto-increments and wraps within its output registers
    class Lsm303dlhcMagnetometerBusAccessI2c
        : public services::RegisterBusAccess
    {
    public:
        static constexpr hal::I2cAddress address{ 0x1e };

        explicit Lsm303dlhcMagnetometerBusAccessI2c(hal::I2cMaster& i2c);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        hal::I2cMasterRegisterAccess<uint8_t> registerAccess;
    };
}

#endif
