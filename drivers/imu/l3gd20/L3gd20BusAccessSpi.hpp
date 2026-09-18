#ifndef DRIVERS_IMU_L3GD20_L3GD20_BUS_ACCESS_SPI_HPP
#define DRIVERS_IMU_L3GD20_L3GD20_BUS_ACCESS_SPI_HPP

#include "hal/interfaces/Spi.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/util/RegisterBusAccess.hpp"

namespace drivers
{
    // Wrap the master in a services::SpiMasterWithChipSelect before handing it here; this adapter
    // drives no chip select of its own.
    class L3gd20BusAccessSpi
        : public services::RegisterBusAccess
    {
    public:
        explicit L3gd20BusAccessSpi(hal::SpiMaster& spiWithChipSelect);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        // Bit 7 selects a read, bit 6 makes the device step through consecutive registers; without it
        // every clocked byte returns the same register
        static constexpr uint8_t readFlag = 0x80;
        static constexpr uint8_t multipleByte = 0x40;

        static uint8_t AddressByte(uint8_t address, std::size_t size, uint8_t direction);

        hal::SpiMaster& spi;
        uint8_t addressByte = 0;
        infra::ByteRange readData;
        infra::ConstByteRange writeData;
        infra::AutoResetFunction<void()> onDone;
    };
}

#endif
