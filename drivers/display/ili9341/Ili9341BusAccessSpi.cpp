#include "drivers/display/ili9341/Ili9341BusAccessSpi.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    namespace
    {
        constexpr bool commandLevel = false;
        constexpr bool dataLevel = true;
    }

    Ili9341BusAccessSpi::Ili9341BusAccessSpi(hal::SpiMaster& spiWithChipSelect, hal::GpioPin& dataCommand)
        : spi(spiWithChipSelect)
        , dataCommand(dataCommand, dataLevel)
    {}

    void Ili9341BusAccessSpi::ReadRegister(uint8_t, infra::ByteRange, const infra::Function<void()>&)
    {
        really_assert(false);
    }

    void Ili9341BusAccessSpi::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        really_assert(!this->onDone);
        command = address;
        parameters = data;
        this->onDone = onDone;

        dataCommand.Set(commandLevel);
        spi.SendData(infra::MakeByteRange(command), parameters.empty() ? hal::SpiAction::stop : hal::SpiAction::continueSession, [this]()
            {
                dataCommand.Set(dataLevel);
                SendParameters();
            });
    }

    void Ili9341BusAccessSpi::SendParameters()
    {
        if (parameters.empty())
            onDone();
        else
            spi.SendData(parameters, hal::SpiAction::stop, [this]()
                {
                    onDone();
                });
    }
}
