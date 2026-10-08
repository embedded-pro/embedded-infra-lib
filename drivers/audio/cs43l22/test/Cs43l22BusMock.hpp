#ifndef DRIVERS_AUDIO_CS43L22_TEST_CS43L22_BUS_MOCK_HPP
#define DRIVERS_AUDIO_CS43L22_TEST_CS43L22_BUS_MOCK_HPP

#include "infra/event/EventDispatcher.hpp"
#include "infra/util/Function.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include "gmock/gmock.h"
#include <cstddef>
#include <cstdint>

namespace drivers
{
    class Cs43l22BusMock
        : public services::RegisterBusAccess
    {
    public:
        void ReadRegister(uint8_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override
        {
            ASSERT_EQ(std::size_t(1), data.size());

            data[0] = ReadRegisterMock(address);

            Finish(onDone);
        }

        void WriteRegister(uint8_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override
        {
            ASSERT_EQ(std::size_t(1), data.size());

            WriteRegisterMock(address, data[0]);

            Finish(onDone);
        }

        bool completeAutomatically = true;

        void CompletePending()
        {
            infra::Function<void()> completion = pending;
            pending = nullptr;
            completion();
        }

        MOCK_METHOD(uint8_t, ReadRegisterMock, (uint8_t address));
        MOCK_METHOD(void, WriteRegisterMock, (uint8_t address, uint8_t value));

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
