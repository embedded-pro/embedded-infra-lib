#ifndef SERVICES_WINDOWED_PROTOCOL_TYPES_HPP
#define SERVICES_WINDOWED_PROTOCOL_TYPES_HPP

#include "infra/util/Aligned.hpp"
#include "infra/util/Endian.hpp"
#include <cstdint>

namespace services
{
    enum class WindowedProtocolOperation : uint8_t
    {
        init = 1,
        initResponse,
        releaseWindow,
        message
    };

    struct WindowedProtocolPacketInit
    {
        explicit WindowedProtocolPacketInit(uint16_t window)
            : window(window)
        {}

        WindowedProtocolOperation operation = WindowedProtocolOperation::init;
        infra::Aligned<uint8_t, infra::LittleEndian<uint16_t>> window;
    };

    struct WindowedProtocolPacketInitResponse
    {
        explicit WindowedProtocolPacketInitResponse(uint16_t window)
            : window(window)
        {}

        WindowedProtocolOperation operation = WindowedProtocolOperation::initResponse;
        infra::Aligned<uint8_t, infra::LittleEndian<uint16_t>> window;
    };

    struct WindowedProtocolPacketReleaseWindow
    {
        explicit WindowedProtocolPacketReleaseWindow(uint16_t window)
            : window(window)
        {}

        WindowedProtocolOperation operation = WindowedProtocolOperation::releaseWindow;
        infra::Aligned<uint8_t, infra::LittleEndian<uint16_t>> window;
    };
}

#endif
