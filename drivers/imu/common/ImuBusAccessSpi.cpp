#include "drivers/imu/common/ImuBusAccessSpi.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    ImuBusAccessSpi::ImuBusAccessSpi(hal::SpiMaster& spiWithChipSelect, uint8_t multipleByteFlag)
        : spi(spiWithChipSelect)
        , multipleByteFlag(multipleByteFlag)
    {}

    void ImuBusAccessSpi::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
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

    void ImuBusAccessSpi::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
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

    uint8_t ImuBusAccessSpi::AddressByte(uint8_t address, std::size_t size, uint8_t direction) const
    {
        return static_cast<uint8_t>(address | direction | (size > 1 ? multipleByteFlag : 0));
    }
}
