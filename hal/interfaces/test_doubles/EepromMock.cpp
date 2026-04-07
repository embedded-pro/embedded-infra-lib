#include "hal/interfaces/test_doubles/EepromMock.hpp"

namespace hal
{
    EepromMock::EepromMock(uint32_t size)
        : size(size)
    {}

    uint32_t EepromMock::Size() const
    {
        return size;
    }

    void EepromMock::WriteBuffer(infra::ConstByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        assert(done == nullptr);
        done = onDone;
        writeBufferMock(std::vector<uint8_t>(buffer.begin(), buffer.end()), address);
    }

    void EepromMock::ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone)
    {
        assert(done == nullptr);
        done = onDone;
        std::vector<uint8_t> result = readBufferMock(address);
        assert(result.size() == buffer.size());
        std::copy(result.begin(), result.end(), buffer.begin());
    }

    void EepromMock::Erase(infra::Function<void()> onDone)
    {
        assert(done == nullptr);
        done = onDone;
        eraseMock();
    }

    CleanEepromMock::CleanEepromMock(uint32_t size)
    {
        EXPECT_CALL(*this, Size()).WillRepeatedly(testing::Return(size));
    }
}
