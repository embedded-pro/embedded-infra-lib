#include "services/hil/HilResponse.hpp"

namespace services
{
    HilResponse::Line::Line(HilResponse& response, const char* head, const char* subject)
        : stream(response.StartLine())
    {
        stream << head;

        if (subject != nullptr)
            stream << ' ' << subject;
    }

    HilResponse::Line::~Line()
    {
        stream << "\r\n";
    }

    HilResponse::Line& HilResponse::Line::operator<<(const char* text)
    {
        stream << text;
        return *this;
    }

    HilResponse::Line& HilResponse::Line::operator<<(infra::BoundedConstString text)
    {
        stream << text;
        return *this;
    }

    HilResponse::Line& HilResponse::Line::operator<<(uint32_t value)
    {
        stream << value;
        return *this;
    }

    HilResponse::Line& HilResponse::Line::Hex(infra::ConstByteRange data)
    {
        stream << infra::AsHex(data);
        return *this;
    }

    HilResponse::Line& HilResponse::Line::Pin(HilPinId pin, const HilPinNaming& naming)
    {
        naming.Print(stream, pin);
        return *this;
    }

    HilResponse::HilResponse(services::Tracer& tracer)
        : tracer(tracer)
    {}

    void HilResponse::BeginCommand()
    {
        inCommand = true;
    }

    void HilResponse::EndCommand()
    {
        inCommand = false;
    }

    HilResponse::Line HilResponse::Ok()
    {
        return Line(*this, "OK");
    }

    HilResponse::Line HilResponse::Event(const char* peripheral)
    {
        return Line(*this, "EVT", peripheral);
    }

    void HilResponse::Error(HilStatus status)
    {
        Line(*this, "ERR", ToString(status));
    }

    infra::TextOutputStream HilResponse::StartLine()
    {
        auto stream = tracer.Continue();

        if (!inCommand)
            stream << "\r\n";

        return stream;
    }
}
