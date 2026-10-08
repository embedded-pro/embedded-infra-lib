#ifndef DRIVERS_IMU_COMMON_IMU_BUS_ACCESS_SPI_HPP
#define DRIVERS_IMU_COMMON_IMU_BUS_ACCESS_SPI_HPP

#include "hal/interfaces/Spi.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <cstdint>

namespace drivers
{
    // Wrap the master in a services::SpiMasterWithChipSelect before handing it here; this adapter
    // drives no chip select of its own.
    // multipleByteFlag is ORed into the address byte when size > 1 (e.g. 0x40 for auto-increment).
    class ImuBusAccessSpi
        : public services::RegisterBusAccess
    {
    public:
        static constexpr uint8_t readFlag = 0x80;

        explicit ImuBusAccessSpi(hal::SpiMaster& spiWithChipSelect, uint8_t multipleByteFlag = 0);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        uint8_t AddressByte(uint8_t address, std::size_t size, uint8_t direction) const;

        hal::SpiMaster& spi;
        uint8_t multipleByteFlag;
        uint8_t addressByte = 0;
        infra::ByteRange readData;
        infra::ConstByteRange writeData;
        infra::AutoResetFunction<void()> onDone;
    };
}

#endif
