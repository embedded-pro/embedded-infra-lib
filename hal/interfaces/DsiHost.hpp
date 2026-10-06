#ifndef HAL_DSI_HOST_HPP
#define HAL_DSI_HOST_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <cstddef>
#include <cstdint>

namespace hal
{
    // Sends and receives packets on a MIPI DSI link. The host chooses the packet type from the number of bytes: DCS short
    // write for up to one parameter and long write for more, generic short write for up to two bytes and long write for more.
    // Virtual channel, low-power or high-speed transmission and bus turn-around are the concern of the host.
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
