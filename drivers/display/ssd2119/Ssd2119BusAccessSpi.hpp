#ifndef DRIVERS_DISPLAY_SSD2119_SSD2119_BUS_ACCESS_SPI_HPP
#define DRIVERS_DISPLAY_SSD2119_SSD2119_BUS_ACCESS_SPI_HPP

#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Spi.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <cstdint>

namespace drivers
{
    // Wrap the master in a services::SpiMasterWithChipSelect; this adapter drives no chip select of its own.
    // Registers cannot be read.
    class Ssd2119BusAccessSpi
        : public services::RegisterBusAccess
    {
    public:
        Ssd2119BusAccessSpi(hal::SpiMaster& spiWithChipSelect, hal::GpioPin& dataCommand);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        void SendWriteData();

    private:
        hal::SpiMaster& spi;
        hal::OutputPin dataCommand;
        uint8_t index = 0;
        infra::ConstByteRange writeData;
        infra::AutoResetFunction<void()> onDone;
    };
}

#endif
