#ifndef HAL_DSI_HOST_MOCK_HPP
#define HAL_DSI_HOST_MOCK_HPP

#include "hal/interfaces/DsiHost.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "gmock/gmock.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hal
{
    // With completeAutomatically false an operation stays outstanding until CompletePending(),
    // which is how a slow host is modelled
    class DsiHostMock
        : public DsiHost
    {
    public:
        std::size_t MaxParametersSize() const override;
        void WriteDcs(uint8_t command, infra::ConstByteRange parameters, const infra::Function<void()>& onDone) override;
        void WriteGeneric(infra::ConstByteRange data, const infra::Function<void()>& onDone) override;
        void ReadDcs(uint8_t command, infra::ByteRange data, const infra::Function<void(Result)>& onDone) override;

        bool CompletionPending() const;
        void CompletePending();

        MOCK_METHOD(void, WriteDcsMock, (uint8_t command, std::vector<uint8_t> parameters));
        MOCK_METHOD(void, WriteGenericMock, (std::vector<uint8_t> data));
        MOCK_METHOD(std::vector<uint8_t>, ReadDcsMock, (uint8_t command, std::size_t size));

        std::size_t maxParametersSize = 1024;
        Result readResult = Result::success;
        bool completeAutomatically = true;

    private:
        void BeginOperation(std::size_t size);
        void Finish();

    private:
        infra::AutoResetFunction<void()> pendingWrite;
        infra::AutoResetFunction<void(Result)> pendingRead;
    };

    class DsiVideoStreamMock
        : public DsiVideoStream
    {
    public:
        void Start(const infra::Function<void()>& onDone) override;
        void Stop(const infra::Function<void()>& onDone) override;

        bool CompletionPending() const;
        void CompletePending();

        MOCK_METHOD(void, StartMock, ());
        MOCK_METHOD(void, StopMock, ());

        bool completeAutomatically = true;

    private:
        void Finish(const infra::Function<void()>& onDone);

    private:
        infra::AutoResetFunction<void()> pending;
    };
}

#endif
