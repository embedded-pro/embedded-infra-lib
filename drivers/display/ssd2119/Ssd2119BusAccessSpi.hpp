#ifndef DRIVERS_DISPLAY_SSD2119_SSD2119_BUS_ACCESS_SPI_HPP
#define DRIVERS_DISPLAY_SSD2119_SSD2119_BUS_ACCESS_SPI_HPP

#include "hal/interfaces/Gpio.hpp"
#include "hal/interfaces/Spi.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <cstdint>

namespace drivers
{
    // Writes SSD2119 registers over the 4-wire, 8-bit serial interface: the register index goes out as one
    // byte while the data/command line is low, the data bytes follow while it is high. Wrap the master in a
    // services::SpiMasterWithChipSelect before handing it here; this adapter drives no chip select of its
    // own, and holds the one the master drives for the index and the data of a register together.
    // The data output of the controller is not wired on the BoosterPack, so registers cannot be read.
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
