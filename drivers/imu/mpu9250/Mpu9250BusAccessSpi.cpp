#include "drivers/imu/mpu9250/Mpu9250BusAccessSpi.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    Mpu9250BusAccessSpi::Mpu9250BusAccessSpi(hal::SpiMaster& spiWithChipSelect)
        : spi(spiWithChipSelect)
    {}

    void Mpu9250BusAccessSpi::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        really_assert(!this->onDone);
        this->addressByte = address | readFlag;
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

    void Mpu9250BusAccessSpi::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        really_assert(!this->onDone);
        this->addressByte = address & ~readFlag;
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

    bool Mpu9250BusAccessSpi::RequiresI2cSlaveInterfaceDisabled() const
    {
        return true;
    }
}
