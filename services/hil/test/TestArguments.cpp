#include "services/hil/Arguments.hpp"
#include "gmock/gmock.h"
#include <array>

namespace
{
    const std::array<services::hil::PinAlias, 1> aliases{ {
        { "id0", { 2, 3 }, services::hil::Pull::up },
    } };

    const services::hil::PinNamingDefault naming{ "ABCDEF", 7, infra::MakeRange(aliases) };

    enum class Mode : uint8_t
    {
        in,
        out,
    };

    constexpr std::array<services::hil::Choice<Mode>, 2> modes{ {
        { "in", Mode::in },
        { "out", Mode::out },
    } };
}

TEST(ArgumentsTest, parse_number_accepts_decimal_and_hexadecimal)
{
    EXPECT_EQ(std::optional<uint32_t>(1234), services::hil::ParseNumber("1234"));
    EXPECT_EQ(std::optional<uint32_t>(0x1f), services::hil::ParseNumber("0x1F"));
    EXPECT_EQ(std::optional<uint32_t>(0xffffffff), services::hil::ParseNumber("4294967295"));
}

TEST(ArgumentsTest, parse_number_rejects_invalid_text)
{
    EXPECT_EQ(std::nullopt, services::hil::ParseNumber(""));
    EXPECT_EQ(std::nullopt, services::hil::ParseNumber("0x"));
    EXPECT_EQ(std::nullopt, services::hil::ParseNumber("12a"));
    EXPECT_EQ(std::nullopt, services::hil::ParseNumber("4294967296"));
}

TEST(ArgumentsTest, parse_hex_fills_output)
{
    std::array<uint8_t, 3> output{};
    std::size_t size = 0;

    EXPECT_EQ(services::hil::Status::done, services::hil::ParseHex("a55A01", infra::MakeRange(output), size));
    EXPECT_EQ(3, size);
    EXPECT_EQ((std::array<uint8_t, 3>{ 0xa5, 0x5a, 0x01 }), output);
}

TEST(ArgumentsTest, parse_hex_reports_errors)
{
    std::array<uint8_t, 2> output{};
    std::size_t size = 5;

    EXPECT_EQ(services::hil::Status::done, services::hil::ParseHex("-", infra::MakeRange(output), size));
    EXPECT_EQ(0, size);
    EXPECT_EQ(services::hil::Status::usage, services::hil::ParseHex("abc", infra::MakeRange(output), size));
    EXPECT_EQ(services::hil::Status::usage, services::hil::ParseHex("", infra::MakeRange(output), size));
    EXPECT_EQ(services::hil::Status::usage, services::hil::ParseHex("zz", infra::MakeRange(output), size));
    EXPECT_EQ(services::hil::Status::range, services::hil::ParseHex("010203", infra::MakeRange(output), size));
}

TEST(ArgumentsTest, parse_duty_cycle)
{
    EXPECT_EQ(hal::DutyCycle(0), services::hil::ParseDutyCycle("0"));
    EXPECT_EQ(hal::DutyCycle(hal::DutyCycle::fullScale), services::hil::ParseDutyCycle("100"));
    EXPECT_EQ(hal::DutyCycle(8192), services::hil::ParseDutyCycle("12.5"));
    EXPECT_EQ(hal::DutyCycle(hal::DutyCycle::fullScale), services::hil::ParseDutyCycle("100.0000"));
    EXPECT_EQ(std::nullopt, services::hil::ParseDutyCycle("100.0001"));
    EXPECT_EQ(std::nullopt, services::hil::ParseDutyCycle("0x10"));
    EXPECT_EQ(std::nullopt, services::hil::ParseDutyCycle("1.23456"));
    EXPECT_EQ(std::nullopt, services::hil::ParseDutyCycle("1."));
    EXPECT_EQ(std::nullopt, services::hil::ParseDutyCycle("1.a"));
}

TEST(ArgumentsTest, shape_checks_positional_count_and_keys)
{
    services::hil::Arguments arguments{ "0 1 baud=9600" };
    const std::array<const char*, 2> keys{ { "tx", "baud" } };

    EXPECT_EQ(2, arguments.PositionalCount());
    EXPECT_TRUE(arguments.Shape(2, 2, { "baud" }));
    EXPECT_FALSE(arguments.Shape(1, 1, { "baud" }));
    EXPECT_FALSE(arguments.Shape(3, 4, { "baud" }));
    EXPECT_FALSE(arguments.Shape(2, 2, { "tx" }));
    EXPECT_TRUE(arguments.Shape(2, 2, infra::MakeRange(keys)));
}

TEST(ArgumentsTest, positional_and_key_access)
{
    services::hil::Arguments arguments{ "a key=value b" };

    EXPECT_EQ("a", arguments.Positional(0));
    EXPECT_EQ("b", arguments.Positional(1));
    EXPECT_EQ("", arguments.Positional(2));
    EXPECT_EQ(std::optional<infra::BoundedConstString>("value"), arguments.Key("key"));
    EXPECT_EQ(std::nullopt, arguments.Key("ke"));
    EXPECT_TRUE(arguments.Has("key"));
    EXPECT_FALSE(arguments.Has("other"));
}

TEST(ArgumentsTest, number_reports_usage_and_range)
{
    services::hil::Arguments arguments{ "5 x n=20" };
    uint32_t value = 0;

    services::hil::Status status = services::hil::Status::done;
    arguments.NumberAt(0, value, 0, 10, status);
    EXPECT_EQ(services::hil::Status::done, status);
    EXPECT_EQ(5, value);

    arguments.Number("n", value, 0, 10, status);
    EXPECT_EQ(services::hil::Status::range, status);

    status = services::hil::Status::done;
    arguments.NumberAt(1, value, 0, 10, status);
    EXPECT_EQ(services::hil::Status::usage, status);
}

TEST(ArgumentsTest, accessors_leave_an_error_untouched)
{
    services::hil::Arguments arguments{ "x n=20 flag=1" };
    uint32_t value = 3;
    bool flag = false;

    services::hil::Status status = services::hil::Status::pin;
    arguments.Number("n", value, 0, 100, status);
    arguments.Flag("flag", flag, status);

    EXPECT_EQ(services::hil::Status::pin, status);
    EXPECT_EQ(3, value);
    EXPECT_FALSE(flag);
}

TEST(ArgumentsTest, flag_and_select)
{
    services::hil::Arguments arguments{ "out flag=1 mode=in bad=2" };
    bool flag = false;
    auto mode = Mode::out;
    auto positional = Mode::in;

    services::hil::Status status = services::hil::Status::done;
    arguments.Flag("flag", flag, status);
    arguments.Select("mode", mode, modes, status);
    arguments.SelectAt(0, positional, modes, status);
    EXPECT_EQ(services::hil::Status::done, status);
    EXPECT_TRUE(flag);
    EXPECT_EQ(Mode::in, mode);
    EXPECT_EQ(Mode::out, positional);

    arguments.Flag("bad", flag, status);
    EXPECT_EQ(services::hil::Status::range, status);
}

TEST(ArgumentsTest, select_rejects_unknown_choice)
{
    services::hil::Arguments arguments{ "mode=od" };
    auto mode = Mode::out;

    services::hil::Status status = services::hil::Status::done;
    arguments.Select("mode", mode, modes, status);
    EXPECT_EQ(services::hil::Status::usage, status);
}

TEST(ArgumentsTest, pins_use_naming_and_alias_pull)
{
    services::hil::Arguments arguments{ "PF1 id0 tx=PB0 rx=PZ1" };
    services::hil::PinId pin;
    auto pull = services::hil::Pull::none;
    std::optional<services::hil::PinId> tx;
    std::optional<services::hil::PinId> rx;

    services::hil::Status status = services::hil::Status::done;
    arguments.PinAt(0, naming, pin, status);
    EXPECT_EQ((services::hil::PinId{ 5, 1 }), pin);

    arguments.PinAt(1, naming, pin, pull, status);
    EXPECT_EQ((services::hil::PinId{ 2, 3 }), pin);
    EXPECT_EQ(services::hil::Pull::up, pull);

    arguments.Pin("tx", naming, tx, status);
    EXPECT_EQ((services::hil::PinId{ 1, 0 }), tx);
    EXPECT_EQ(services::hil::Status::done, status);

    arguments.Pin("rx", naming, rx, status);
    EXPECT_EQ(services::hil::Status::pin, status);
}
