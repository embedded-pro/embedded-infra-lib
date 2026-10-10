#include "services/peripheral/I2cScanner.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    I2cScanner::I2cScanner(hal::I2cMaster& i2c)
        : i2c(i2c)
    {}

    void I2cScanner::Scan(const infra::Function<void(hal::I2cAddress address)>& onDeviceFound, const infra::Function<void(uint32_t numberOfDevices)>& onDone)
    {
        really_assert(!scanning);

        this->onDeviceFound = onDeviceFound;
        this->onDone = onDone;
        scanning = true;
        numberOfDevices = 0;
        address = firstAddress;
        Probe();
    }

    bool I2cScanner::Scanning() const
    {
        return scanning;
    }

    void I2cScanner::Probe()
    {
        i2c.ReceiveData(hal::I2cAddress(address), infra::MakeRange(data), hal::Action::stop, [this](hal::Result result)
            {
                Probed(result);
            });
    }

    void I2cScanner::Probed(hal::Result result)
    {
        if (result == hal::Result::complete)
        {
            ++numberOfDevices;
            onDeviceFound(hal::I2cAddress(address));
        }

        if (address++ != lastAddress)
            Probe();
        else
            Finish();
    }

    void I2cScanner::Finish()
    {
        scanning = false;
        onDone(numberOfDevices);
    }
}
