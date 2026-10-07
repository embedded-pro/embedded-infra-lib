#ifndef HAL_DSI_HOST_HPP
#define HAL_DSI_HOST_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstddef>
#include <cstdint>

namespace hal
{
    class DsiHost
    {
    protected:
        DsiHost() = default;
        DsiHost(const DsiHost& other) = delete;
        DsiHost& operator=(const DsiHost& other) = delete;
        ~DsiHost() = default;

    public:
        enum class Result : uint8_t
        {
            success,
            timeout,
            failed
        };

        // At most one operation is in flight; the buffers stay valid until onDone, which is never called from within the call.
        // MaxParametersSize bounds the parameters of one WriteDcs and the data of one WriteGeneric
        virtual std::size_t MaxParametersSize() const = 0;
        virtual void WriteDcs(uint8_t command, infra::ConstByteRange parameters, const infra::Function<void()>& onDone) = 0;
        virtual void WriteGeneric(infra::ConstByteRange data, const infra::Function<void()>& onDone) = 0;
        virtual void ReadDcs(uint8_t command, infra::ByteRange data, const infra::Function<void(Result)>& onDone) = 0;
    };

    // Start is only valid while the stream is stopped, and Stop only while it runs
    class DsiVideoStream
    {
    protected:
        DsiVideoStream() = default;
        DsiVideoStream(const DsiVideoStream& other) = delete;
        DsiVideoStream& operator=(const DsiVideoStream& other) = delete;
        ~DsiVideoStream() = default;

    public:
        virtual void Start(const infra::Function<void()>& onDone) = 0;
        virtual void Stop(const infra::Function<void()>& onDone) = 0;
    };
}

#endif
