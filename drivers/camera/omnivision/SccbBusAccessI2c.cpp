#include "drivers/camera/omnivision/SccbBusAccessI2c.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace drivers
{
    SccbBusAccessI2c::SccbBusAccessI2c(hal::I2cMaster& i2c, hal::I2cAddress address)
        : i2c(i2c)
        , deviceAddress(address)
    {}

    void SccbBusAccessI2c::ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone)
    {
        really_assert(data.size() == 1);
        really_assert(!this->onDone);

        registerByte = address;
        readData = data;
        this->onDone = onDone;

        i2c.SendData(deviceAddress, infra::MakeByteRange(registerByte), hal::Action::stop,
            [this](hal::Result, uint32_t)
            {
                i2c.ReceiveData(deviceAddress, readData, hal::Action::stop,
                    [this](hal::Result)
                    {
                        infra::Function<void()> done = this->onDone;
                        this->onDone = nullptr;
                        done();
                    });
            });
    }

    void SccbBusAccessI2c::WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        really_assert(data.size() == 1);
        really_assert(!this->onDone);

        registerByte = address;
        writeData = data;
        this->onDone = onDone;

        i2c.SendData(deviceAddress, infra::MakeByteRange(registerByte), hal::Action::continueSession,
            [this](hal::Result, uint32_t)
            {
                i2c.SendData(deviceAddress, writeData, hal::Action::stop,
                    [this](hal::Result, uint32_t)
                    {
                        infra::Function<void()> done = this->onDone;
                        this->onDone = nullptr;
                        done();
                    });
            });
    }
}
