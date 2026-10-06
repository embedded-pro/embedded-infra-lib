#include "hal/interfaces/test_doubles/BlockDeviceStub.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace hal
{
    BlockDeviceStub::BlockDeviceStub(infra::ByteRange storage, uint32_t blockSize)
        : storage(storage)
        , blockSize(blockSize)
    {}

    uint32_t BlockDeviceStub::BlockSize() const
    {
        return blockSize;
    }

    uint32_t BlockDeviceStub::NumberOfBlocks() const
    {
        return static_cast<uint32_t>(storage.size() / blockSize);
    }

    void BlockDeviceStub::ReadBlocks(infra::ByteRange buffer, uint32_t firstBlock, const infra::Function<void(Result)>& onDoneCallback)
    {
        really_assert(buffer.size() % blockSize == 0);
        uint32_t blockCount = static_cast<uint32_t>(buffer.size() / blockSize);
        onDone = onDoneCallback;

        if (!IsRangeValid(firstBlock, blockCount))
        {
            ScheduleCompletion(Result::outOfRange);
            return;
        }

        if (auto failure = ConsumeInjectedFailure())
        {
            ScheduleCompletion(*failure);
            return;
        }

        std::copy(storage.begin() + firstBlock * blockSize,
            storage.begin() + (firstBlock + blockCount) * blockSize,
            buffer.begin());

        ScheduleCompletion(Result::success);
    }

    void BlockDeviceStub::WriteBlocks(infra::ConstByteRange buffer, uint32_t firstBlock, const infra::Function<void(Result)>& onDoneCallback)
    {
        really_assert(buffer.size() % blockSize == 0);
        uint32_t blockCount = static_cast<uint32_t>(buffer.size() / blockSize);
        onDone = onDoneCallback;

        if (!IsRangeValid(firstBlock, blockCount))
        {
            ScheduleCompletion(Result::outOfRange);
            return;
        }

        if (auto failure = ConsumeInjectedFailure())
        {
            ScheduleCompletion(*failure);
            return;
        }

        std::copy(buffer.begin(), buffer.end(),
            storage.begin() + firstBlock * blockSize);

        ScheduleCompletion(Result::success);
    }

    void BlockDeviceStub::EraseBlocks(uint32_t beginBlock, uint32_t endBlock, const infra::Function<void(Result)>& onDoneCallback)
    {
        onDone = onDoneCallback;

        if (beginBlock > endBlock || !IsRangeValid(beginBlock, endBlock - beginBlock))
        {
            ScheduleCompletion(Result::outOfRange);
            return;
        }

        if (auto failure = ConsumeInjectedFailure())
        {
            ScheduleCompletion(*failure);
            return;
        }

        std::fill(storage.begin() + beginBlock * blockSize,
            storage.begin() + endBlock * blockSize,
            uint8_t{ 0x00 });

        ScheduleCompletion(Result::success);
    }

    void BlockDeviceStub::FailNextOperationWith(Result result)
    {
        injectedFailure = result;
    }

    bool BlockDeviceStub::IsRangeValid(uint32_t firstBlock, uint32_t blockCount) const
    {
        if (firstBlock > NumberOfBlocks())
            return false;

        return blockCount <= NumberOfBlocks() - firstBlock;
    }

    auto BlockDeviceStub::ConsumeInjectedFailure() -> std::optional<Result>
    {
        if (injectedFailure)
        {
            auto result = *injectedFailure;
            injectedFailure = std::nullopt;
            return result;
        }

        return std::nullopt;
    }

    void BlockDeviceStub::ScheduleCompletion(Result result)
    {
        infra::EventDispatcher::Instance().Schedule([this, result]
            {
                onDone(result);
            });
    }
}
