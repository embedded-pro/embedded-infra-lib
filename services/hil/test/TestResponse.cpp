#include "infra/stream/StringOutputStream.hpp"
#include "services/hil/Response.hpp"
#include "services/tracer/Tracer.hpp"
#include "gmock/gmock.h"
#include <array>

class ResponseTest
    : public testing::Test
{
public:
    infra::StringOutputStream::WithStorage<256> stream;
    services::TracerToStream tracer{ stream };
    services::hil::Response response{ tracer };
    services::hil::PinNamingDefault naming{ "ABCDEFGHJKLMNPQ", 7 };
};

TEST_F(ResponseTest, line_inside_a_command_has_no_prefix)
{
    response.BeginCommand();
    response.Ok() << " value=" << 12u << " text=" << infra::BoundedConstString("abc");
    response.EndCommand();

    EXPECT_EQ("OK value=12 text=abc\r\n", stream.Storage());
}

TEST_F(ResponseTest, line_outside_a_command_starts_on_a_new_line)
{
    response.Ok();

    EXPECT_EQ("\r\nOK\r\n", stream.Storage());
}

TEST_F(ResponseTest, event_names_the_peripheral)
{
    response.Event("can") << " index=" << 0u;

    EXPECT_EQ("\r\nEVT can index=0\r\n", stream.Storage());
}

TEST_F(ResponseTest, error_prints_reason_token)
{
    response.BeginCommand();
    response.Error(services::hil::Status::usage);
    response.Error(services::hil::Status::pin);
    response.Error(services::hil::Status::busy);
    response.Error(services::hil::Status::notOpen);
    response.Error(services::hil::Status::unsupported);
    response.Error(services::hil::Status::range);
    response.Error(services::hil::Status::timeout);
    response.Error(services::hil::Status::failed);
    response.EndCommand();

    EXPECT_EQ("ERR usage\r\nERR pin\r\nERR busy\r\nERR notopen\r\nERR unsupported\r\nERR range\r\nERR timeout\r\nERR failed\r\n", stream.Storage());
}

TEST_F(ResponseTest, hex_is_lowercase_with_two_digits_per_byte)
{
    const std::array<uint8_t, 4> data{ { 0x0a, 0xff, 0x00, 0x5A } };

    response.BeginCommand();
    (response.Ok() << " data=").Hex(infra::MakeRange(data)) << " next=" << 255u;
    response.EndCommand();

    EXPECT_EQ("OK data=0aff005a next=255\r\n", stream.Storage());
}

TEST_F(ResponseTest, empty_hex_prints_nothing)
{
    response.BeginCommand();
    (response.Ok() << " data=").Hex(infra::ConstByteRange());
    response.EndCommand();

    EXPECT_EQ("OK data=\r\n", stream.Storage());
}

TEST_F(ResponseTest, pin_uses_naming)
{
    response.BeginCommand();
    (response.Ok() << " pin=").Pin(services::hil::PinId{ 8, 0 }, naming);
    response.EndCommand();

    EXPECT_EQ("OK pin=PJ0\r\n", stream.Storage());
}
