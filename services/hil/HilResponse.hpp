#ifndef SERVICES_HIL_RESPONSE_HPP
#define SERVICES_HIL_RESPONSE_HPP

#include "infra/stream/OutputStream.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/ByteRange.hpp"
#include "services/hil/HilPinId.hpp"
#include "services/hil/HilPinNaming.hpp"
#include "services/hil/HilStatus.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>

namespace services
{
    class HilResponse
    {
    public:
        class Line
        {
        public:
            Line(HilResponse& response, const char* head, const char* subject = nullptr);
            Line(const Line& other) = delete;
            Line& operator=(const Line& other) = delete;
            ~Line();

            Line& operator<<(const char* text);
            Line& operator<<(infra::BoundedConstString text);
            Line& operator<<(uint32_t value);
            Line& Hex(infra::ConstByteRange data);
            Line& Pin(HilPinId pin, const HilPinNaming& naming);

        private:
            infra::TextOutputStream stream;
        };

        explicit HilResponse(services::Tracer& tracer);

        void BeginCommand();
        void EndCommand();

        Line Ok();
        Line Event(const char* peripheral);
        void Error(HilStatus status);

    private:
        infra::TextOutputStream StartLine();

    private:
        services::Tracer& tracer;
        bool inCommand = false;
    };
}

#endif
