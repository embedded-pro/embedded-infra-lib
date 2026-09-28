#include "services/hil/Response.hpp"

namespace services::hil
{
    Response::Line::Line(Response& response, const char* head, const char* subject)
        : stream(response.StartLine())
    {
        stream << head;

        if (subject != nullptr)
            stream << ' ' << subject;
    }

    Response::Line::~Line()
    {
        stream << "\r\n";
    }

    Response::Line& Response::Line::operator<<(const char* text)
    {
        stream << text;
        return *this;
    }

    Response::Line& Response::Line::operator<<(infra::BoundedConstString text)
    {
        stream << text;
        return *this;
    }

    Response::Line& Response::Line::operator<<(uint32_t value)
    {
        stream << value;
        return *this;
    }

    Response::Line& Response::Line::Hex(infra::ConstByteRange data)
    {
        stream << infra::AsHex(data);
        return *this;
    }

    Response::Line& Response::Line::Pin(PinId pin, const PinNaming& naming)
    {
        naming.Print(stream, pin);
        return *this;
    }

    Response::Response(services::Tracer& tracer)
        : tracer(tracer)
    {}

    void Response::BeginCommand()
    {
        inCommand = true;
    }

    void Response::EndCommand()
    {
        inCommand = false;
    }

    Response::Line Response::Ok()
    {
        return Line(*this, "OK");
    }

    Response::Line Response::Event(const char* peripheral)
    {
        return Line(*this, "EVT", peripheral);
    }

    void Response::Error(Status status)
    {
        Line(*this, "ERR", ToString(status));
    }

    infra::TextOutputStream Response::StartLine()
    {
        auto stream = tracer.Continue();

        if (!inCommand)
            stream << "\r\n";

        return stream;
    }
}
