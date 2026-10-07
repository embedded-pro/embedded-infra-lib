#ifndef DRIVERS_DISPLAY_ILI9341_ILI9341_BUS_ACCESS_SPI_HPP
#define DRIVERS_DISPLAY_ILI9341_ILI9341_BUS_ACCESS_SPI_HPP

#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Spi.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <cstdint>

namespace drivers
{
    // The 4-line serial interface: the data/command line is low for the command byte and high for its parameters.
    // Wrap the master in a services::SpiMasterWithChipSelect when the chip select is a plain pin;
    // this adapter drives no chip select of its own. Registers cannot be read.
    class Ili9341BusAccessSpi
        : public services::RegisterBusAccess
    {
    public:
        Ili9341BusAccessSpi(hal::SpiMaster& spiWithChipSelect, hal::GpioPin& dataCommand);

        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override;
        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        void SendParameters();

    private:
        hal::SpiMaster& spi;
        hal::OutputPin dataCommand;
        uint8_t command = 0;
        infra::ConstByteRange parameters;
        infra::AutoResetFunction<void()> onDone;
    };
}

#endif
