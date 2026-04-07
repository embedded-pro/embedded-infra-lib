#include "hal/interfaces/test_doubles/EepromStub.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace hal
{
    EepromStub::EepromStub(uint32_t size)
        : storage(size, 0xff)
    {}

    uint32_t EepromStub::Size() const
    {
        return static_cast<uint32_t>(storage.size());
    }

    void EepromStub::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        std::copy(buffer.begin(), buffer.end(), storage.begin() + address);
        infra::EventDispatcher::Instance().Schedule(onDone);
    }

    void EepromStub::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        std::copy(storage.begin() + address, storage.begin() + address + buffer.size(), buffer.begin());
        infra::EventDispatcher::Instance().Schedule(onDone);
    }

    void EepromStub::Erase(infra::Function<void()> onDone)
    {
        std::fill(storage.begin(), storage.end(), 0xff);
        infra::EventDispatcher::Instance().Schedule(onDone);
    }
}
