#include "drivers/display/ssd2119/Ssd2119BusAccessSpi.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    namespace
    {
        constexpr bool commandLevel = false;
        constexpr bool dataLevel = true;
    }

    Ssd2119BusAccessSpi::Ssd2119BusAccessSpi(hal::SpiMaster& spiWithChipSelect, hal::GpioPin& dataCommand)
        : spi(spiWithChipSelect)
        , dataCommand(dataCommand, dataLevel)
    {}

    void Ssd2119BusAccessSpi::ReadRegister(uint8_t, infra::ByteRange, const infra::Function<void()>&)
    {
        really_assert(false);
    }

    void Ssd2119BusAccessSpi::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        really_assert(!this->onDone);
        index = address;
        writeData = data;
        this->onDone = onDone;

        dataCommand.Set(commandLevel);
        spi.SendData(infra::MakeByteRange(index), writeData.empty() ? hal::SpiAction::stop : hal::SpiAction::continueSession, [this]()
            {
                dataCommand.Set(dataLevel);
                SendWriteData();
            });
    }

    void Ssd2119BusAccessSpi::SendWriteData()
    {
        if (writeData.empty())
            onDone();
        else
            spi.SendData(writeData, hal::SpiAction::stop, [this]()
                {
                    onDone();
                });
    }
}
