#ifndef DRIVERS_IMU_MPU9250_MPU9250_BUS_ACCESS_SPI_HPP
#define DRIVERS_IMU_MPU9250_MPU9250_BUS_ACCESS_SPI_HPP

#include "drivers/imu/mpu9250/Mpu9250BusAccess.hpp"
#include "hal/interfaces/Spi.hpp"
#include "infra/util/AutoResetFunction.hpp"

namespace drivers
{
    // This adapter does not select a clock. The part accepts 20 MHz only while reading the sensor and
    // interrupt registers and is limited to 1 MHz elsewhere, so a bus shared across both must be
    // clocked at 1 MHz, or the caller must switch it per transaction.
    class Mpu9250BusAccessSpi
        : public Mpu9250BusAccess
    {
    public:
        explicit Mpu9250BusAccessSpi(hal::SpiMaster& spiWithChipSelect);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;
        bool RequiresI2cSlaveInterfaceDisabled() const override;

    private:
        static constexpr uint8_t readFlag = 0x80;

        hal::SpiMaster& spi;
        uint8_t addressByte = 0;
        infra::ByteRange readData;
        infra::ConstByteRange writeData;
        infra::AutoResetFunction<void()> onDone;
    };
}

#endif
