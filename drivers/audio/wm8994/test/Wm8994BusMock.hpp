#ifndef DRIVERS_AUDIO_WM8994_TEST_WM8994_BUS_MOCK_HPP
#define DRIVERS_AUDIO_WM8994_TEST_WM8994_BUS_MOCK_HPP

#include "infra/event/EventDispatcher.hpp"
#include "infra/util/Function.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include "gmock/gmock.h"
#include <cstdint>

namespace drivers
{
    class Wm8994BusMock
        : public services::RegisterBusAccessHalfWord
    {
    public:
        void ReadRegister(uint16_t address, infra::ByteRange data, const infra::Function<void()>& onDone) override
        {
            ASSERT_EQ(std::size_t(2), data.size());

            const uint16_t value = ReadRegisterMock(address);
            data[0] = static_cast<uint8_t>(value >> 8);
            data[1] = static_cast<uint8_t>(value);

            Finish(onDone);
        }

        void WriteRegister(uint16_t address, infra::ConstByteRange data, const infra::Function<void()>& onDone) override
        {
            ASSERT_EQ(std::size_t(2), data.size());

            WriteRegisterMock(address, static_cast<uint16_t>((data[0] << 8) | data[1]));

            Finish(onDone);
        }

        bool completeAutomatically = true;

        void CompletePending()
        {
            infra::Function<void()> completion = pending;
            pending = nullptr;
            completion();
        }

        MOCK_METHOD(uint16_t, ReadRegisterMock, (uint16_t address));
        MOCK_METHOD(void, WriteRegisterMock, (uint16_t address, uint16_t value));

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
