#include "services/hil/HilArguments.hpp"
#include "gmock/gmock.h"
#include <array>

namespace
{
    const std::array<services::HilPinAlias, 1> aliases{ {
        { "id0", { 2, 3 }, services::HilPull::up },
    } };

    const services::HilPinNamingDefault naming{ "ABCDEF", 7, infra::MakeRange(aliases) };

    enum class Mode : uint8_t
    {
        in,
        out,
    };

    constexpr std::array<services::HilChoice<Mode>, 2> modes{ {
        { "in", Mode::in },
        { "out", Mode::out },
    } };
}

TEST(HilArgumentsTest, parse_number_accepts_decimal_and_hexadecimal)
{
    EXPECT_EQ(std::optional<uint32_t>(1234), services::HilArguments::ParseNumber("1234"));
    EXPECT_EQ(std::optional<uint32_t>(0x1f), services::HilArguments::ParseNumber("0x1F"));
    EXPECT_EQ(std::optional<uint32_t>(0xffffffff), services::HilArguments::ParseNumber("4294967295"));
}

TEST(HilArgumentsTest, parse_number_rejects_invalid_text)
{
    EXPECT_EQ(std::nullopt, services::HilArguments::ParseNumber(""));
    EXPECT_EQ(std::nullopt, services::HilArguments::ParseNumber("0x"));
    EXPECT_EQ(std::nullopt, services::HilArguments::ParseNumber("12a"));
    EXPECT_EQ(std::nullopt, services::HilArguments::ParseNumber("4294967296"));
}

TEST(HilArgumentsTest, parse_hex_fills_output)
{
    std::array<uint8_t, 3> output{};
    std::size_t size = 0;

    EXPECT_EQ(services::HilStatus::done, services::HilArguments::ParseHex("a55A01", infra::MakeRange(output), size));
    EXPECT_EQ(3, size);
    EXPECT_EQ((std::array<uint8_t, 3>{ 0xa5, 0x5a, 0x01 }), output);
}

TEST(HilArgumentsTest, parse_hex_reports_errors)
{
    std::array<uint8_t, 2> output{};
    std::size_t size = 5;

    EXPECT_EQ(services::HilStatus::done, services::HilArguments::ParseHex("-", infra::MakeRange(output), size));
    EXPECT_EQ(0, size);
    EXPECT_EQ(services::HilStatus::usage, services::HilArguments::ParseHex("abc", infra::MakeRange(output), size));
    EXPECT_EQ(services::HilStatus::usage, services::HilArguments::ParseHex("", infra::MakeRange(output), size));
    EXPECT_EQ(services::HilStatus::usage, services::HilArguments::ParseHex("zz", infra::MakeRange(output), size));
    EXPECT_EQ(services::HilStatus::range, services::HilArguments::ParseHex("010203", infra::MakeRange(output), size));
}

TEST(HilArgumentsTest, parse_duty_cycle)
{
    EXPECT_EQ(hal::DutyCycle(0), services::HilArguments::ParseDutyCycle("0"));
    EXPECT_EQ(hal::DutyCycle(hal::DutyCycle::fullScale), services::HilArguments::ParseDutyCycle("100"));
    EXPECT_EQ(hal::DutyCycle(8192), services::HilArguments::ParseDutyCycle("12.5"));
    EXPECT_EQ(hal::DutyCycle(hal::DutyCycle::fullScale), services::HilArguments::ParseDutyCycle("100.0000"));
    EXPECT_EQ(std::nullopt, services::HilArguments::ParseDutyCycle("100.0001"));
    EXPECT_EQ(std::nullopt, services::HilArguments::ParseDutyCycle("0x10"));
    EXPECT_EQ(std::nullopt, services::HilArguments::ParseDutyCycle("1.23456"));
    EXPECT_EQ(std::nullopt, services::HilArguments::ParseDutyCycle("1."));
    EXPECT_EQ(std::nullopt, services::HilArguments::ParseDutyCycle("1.a"));
}

TEST(HilArgumentsTest, shape_checks_positional_count_and_keys)
{
    services::HilArguments arguments{ "0 1 baud=9600" };
    const std::array<const char*, 2> keys{ { "tx", "baud" } };

    EXPECT_EQ(2, arguments.PositionalCount());
    EXPECT_TRUE(arguments.Shape(2, 2, { "baud" }));
    EXPECT_FALSE(arguments.Shape(1, 1, { "baud" }));
    EXPECT_FALSE(arguments.Shape(3, 4, { "baud" }));
    EXPECT_FALSE(arguments.Shape(2, 2, { "tx" }));
    EXPECT_TRUE(arguments.Shape(2, 2, infra::MakeRange(keys)));
}

TEST(HilArgumentsTest, positional_and_key_access)
{
    services::HilArguments arguments{ "a key=value b" };

    EXPECT_EQ("a", arguments.Positional(0));
    EXPECT_EQ("b", arguments.Positional(1));
    EXPECT_EQ("", arguments.Positional(2));
    EXPECT_EQ(std::optional<infra::BoundedConstString>("value"), arguments.Key("key"));
    EXPECT_EQ(std::nullopt, arguments.Key("ke"));
    EXPECT_TRUE(arguments.Has("key"));
    EXPECT_FALSE(arguments.Has("other"));
}

TEST(HilArgumentsTest, number_reports_usage_and_range)
{
    services::HilArguments arguments{ "5 x n=20" };
    uint32_t value = 0;

    services::HilStatus status = services::HilStatus::done;
    arguments.NumberAt(0, value, 0, 10, status);
    EXPECT_EQ(services::HilStatus::done, status);
    EXPECT_EQ(5, value);

    arguments.Number("n", value, 0, 10, status);
    EXPECT_EQ(services::HilStatus::range, status);

    status = services::HilStatus::done;
    arguments.NumberAt(1, value, 0, 10, status);
    EXPECT_EQ(services::HilStatus::usage, status);
}

TEST(HilArgumentsTest, accessors_leave_an_error_untouched)
{
    services::HilArguments arguments{ "x n=20 flag=1" };
    uint32_t value = 3;
    bool flag = false;

    services::HilStatus status = services::HilStatus::pin;
    arguments.Number("n", value, 0, 100, status);
    arguments.Flag("flag", flag, status);

    EXPECT_EQ(services::HilStatus::pin, status);
    EXPECT_EQ(3, value);
    EXPECT_FALSE(flag);
}

TEST(HilArgumentsTest, flag_and_select)
{
    services::HilArguments arguments{ "out flag=1 mode=in bad=2" };
    bool flag = false;
    auto mode = Mode::out;
    auto positional = Mode::in;

    services::HilStatus status = services::HilStatus::done;
    arguments.Flag("flag", flag, status);
    arguments.Select("mode", mode, modes, status);
    arguments.SelectAt(0, positional, modes, status);
    EXPECT_EQ(services::HilStatus::done, status);
    EXPECT_TRUE(flag);
    EXPECT_EQ(Mode::in, mode);
    EXPECT_EQ(Mode::out, positional);

    arguments.Flag("bad", flag, status);
    EXPECT_EQ(services::HilStatus::range, status);
}

TEST(HilArgumentsTest, select_rejects_unknown_choice)
{
    services::HilArguments arguments{ "mode=od" };
    auto mode = Mode::out;

    services::HilStatus status = services::HilStatus::done;
    arguments.Select("mode", mode, modes, status);
    EXPECT_EQ(services::HilStatus::usage, status);
}

TEST(HilArgumentsTest, pins_use_naming_and_alias_pull)
{
    services::HilArguments arguments{ "PF1 id0 tx=PB0 rx=PZ1" };
    services::HilPinId pin;
    auto pull = services::HilPull::none;
    std::optional<services::HilPinId> tx;
    std::optional<services::HilPinId> rx;

    services::HilStatus status = services::HilStatus::done;
    arguments.PinAt(0, naming, pin, status);
    EXPECT_EQ((services::HilPinId{ 5, 1 }), pin);

    arguments.PinAt(1, naming, pin, pull, status);
    EXPECT_EQ((services::HilPinId{ 2, 3 }), pin);
    EXPECT_EQ(services::HilPull::up, pull);

    arguments.Pin("tx", naming, tx, status);
    EXPECT_EQ((services::HilPinId{ 1, 0 }), tx);
    EXPECT_EQ(services::HilStatus::done, status);

    arguments.Pin("rx", naming, rx, status);
    EXPECT_EQ(services::HilStatus::pin, status);
}
