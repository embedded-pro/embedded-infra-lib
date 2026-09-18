#include "drivers/imu/l3gd20/L3gd20BusAccessSpi.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    L3gd20BusAccessSpi::L3gd20BusAccessSpi(hal::SpiMaster& spiWithChipSelect)
        : spi(spiWithChipSelect)
    {}

    void L3gd20BusAccessSpi::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        really_assert(!this->onDone);
        this->addressByte = AddressByte(address, data.size(), readFlag);
        this->readData = data;
        this->onDone = onDone;

        spi.SendData(infra::MakeByteRange(addressByte), hal::SpiAction::continueSession, [this]()
            {
                spi.ReceiveData(readData, hal::SpiAction::stop, [this]()
                    {
                        this->onDone();
                    });
            });
    }

    void L3gd20BusAccessSpi::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        really_assert(!this->onDone);
        this->addressByte = AddressByte(address, data.size(), 0);
        this->writeData = data;
        this->onDone = onDone;

        spi.SendData(infra::MakeByteRange(addressByte), hal::SpiAction::continueSession, [this]()
            {
                spi.SendData(writeData, hal::SpiAction::stop, [this]()
                    {
                        this->onDone();
                    });
            });
    }

    uint8_t L3gd20BusAccessSpi::AddressByte(uint8_t address, std::size_t size, uint8_t direction)
    {
        return static_cast<uint8_t>(address | direction | (size > 1 ? multipleByte : 0));
    }
}
