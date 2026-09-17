#ifndef DRIVERS_IMU_MPU9250_MPU9250_BUS_ACCESS_MOCK_HPP
#define DRIVERS_IMU_MPU9250_MPU9250_BUS_ACCESS_MOCK_HPP

#include "drivers/imu/mpu9250/Mpu9250BusAccess.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "gmock/gmock.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace drivers
{
    class Mpu9250BusAccessMock
        : public Mpu9250BusAccess
    {
    public:
        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override
        {
            std::vector<uint8_t> received = ReadRegisterMock(address, data.size());
            EXPECT_EQ(received.size(), data.size());
            std::copy(received.begin(), received.end(), data.begin());

            Finish(onDone);
        }

        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override
        {
            WriteRegisterMock(address, std::vector<uint8_t>(data.begin(), data.end()));

            Finish(onDone);
        }

        // With completeAutomatically false the transaction stays outstanding until CompletePending(),
        // which is how a slow bus is modelled
        bool completeAutomatically = true;

        bool CompletionPending() const
        {
            return static_cast<bool>(pending);
        }

        void CompletePending()
        {
            infra::Function<void()> completion = pending;
            pending = nullptr;
            completion();
        }

        MOCK_METHOD(std::vector<uint8_t>, ReadRegisterMock, (uint8_t address, std::size_t size));
        MOCK_METHOD(void, WriteRegisterMock, (uint8_t address, std::vector<uint8_t> data));
        MOCK_METHOD(bool, RequiresI2cSlaveInterfaceDisabled, (), (const, override));

    private:
        void Finish(const infra::Function<void()>& onDone)
        {
            if (completeAutomatically)
                infra::EventDispatcher::Instance().Schedule(onDone);
            else
                pending = onDone;
        }

        infra::Function<void()> pending;
    };
}

#endif
