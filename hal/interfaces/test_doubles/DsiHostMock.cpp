#include "hal/interfaces/test_doubles/DsiHostMock.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace hal
{
    DsiHostMock::DsiHostMock() = default;

    std::size_t DsiHostMock::MaxParametersSize() const
    {
        return maxParametersSize;
    }

    void DsiHostMock::WriteDcs(uint8_t command, infra::ConstByteRange parameters, const infra::Function<void()>& onDone)
    {
        BeginOperation(parameters.size());
        WriteDcsMock(command, std::vector<uint8_t>(parameters.begin(), parameters.end()));

        pendingWrite = onDone;
        Finish();
    }

    void DsiHostMock::WriteGeneric(infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        BeginOperation(data.size());
        WriteGenericMock(std::vector<uint8_t>(data.begin(), data.end()));

        pendingWrite = onDone;
        Finish();
    }

    void DsiHostMock::ReadDcs(uint8_t command, infra::ByteRange data, const infra::Function<void(Result)>& onDone)
    {
        BeginOperation(0);
        std::vector<uint8_t> received = ReadDcsMock(command, data.size());

        if (readResult == Result::success)
        {
            EXPECT_EQ(received.size(), data.size());
            std::copy(received.begin(), received.begin() + std::min(received.size(), data.size()), data.begin());
        }

        pendingRead = onDone;
        Finish();
    }

    bool DsiHostMock::CompletionPending() const
    {
        return static_cast<bool>(pendingWrite) || static_cast<bool>(pendingRead);
    }

    void DsiHostMock::CompletePending()
    {
        really_assert(CompletionPending());

        if (pendingRead)
            pendingRead(readResult);
        else
            pendingWrite();
    }

    void DsiHostMock::BeginOperation(std::size_t size)
    {
        really_assert(!CompletionPending());
        really_assert(size <= maxParametersSize);
    }

    void DsiHostMock::Finish()
    {
        if (completeAutomatically)
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    if (CompletionPending())
                        CompletePending();
                });
    }

    DsiVideoStreamMock::DsiVideoStreamMock() = default;

    void DsiVideoStreamMock::Start(const infra::Function<void()>& onDone)
    {
        really_assert(!CompletionPending());
        StartMock();

        Finish(onDone);
    }

    void DsiVideoStreamMock::Stop(const infra::Function<void()>& onDone)
    {
        really_assert(!CompletionPending());
        StopMock();

        Finish(onDone);
    }

    bool DsiVideoStreamMock::CompletionPending() const
    {
        return static_cast<bool>(pending);
    }

    void DsiVideoStreamMock::CompletePending()
    {
        really_assert(CompletionPending());
        pending();
    }

    void DsiVideoStreamMock::Finish(const infra::Function<void()>& onDone)
    {
        pending = onDone;

        if (completeAutomatically)
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    if (CompletionPending())
                        CompletePending();
                });
    }
}
