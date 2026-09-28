#include "infra/stream/StringOutputStream.hpp"
#include "services/hil/PinNaming.hpp"
#include "gmock/gmock.h"
#include <array>

namespace
{
    const std::array<services::hil::PinAlias, 2> aliases{ {
        { "led", { 5, 1 } },
        { "id0", { 9, 0 }, services::hil::Pull::up },
    } };
}

class PinNamingDefaultTest
    : public testing::Test
{
public:
    std::optional<services::hil::PinId> Parse(const services::hil::PinNaming& naming, infra::BoundedConstString text)
    {
        return naming.Parse(text, pull);
    }

    std::string Print(const services::hil::PinNaming& naming, services::hil::PinId pin)
    {
        infra::StringOutputStream::WithStorage<16> stream;
        naming.Print(stream, pin);
        return std::string(stream.Storage().begin(), stream.Storage().end());
    }

    services::hil::Pull pull = services::hil::Pull::down;
    services::hil::PinNamingDefault tiva{ "ABCDEFGHJKLMNPQ", 7, infra::MakeRange(aliases) };
    services::hil::PinNamingDefault stm32{ "ABCDEFGHIJK", 15 };
};

TEST_F(PinNamingDefaultTest, parses_tiva_pins)
{
    EXPECT_EQ((services::hil::PinId{ 5, 1 }), Parse(tiva, "PF1"));
    EXPECT_EQ((services::hil::PinId{ 8, 0 }), Parse(tiva, "PJ0"));
    EXPECT_EQ((services::hil::PinId{ 14, 3 }), Parse(tiva, "pq3"));
    EXPECT_EQ(services::hil::Pull::down, pull);
}

TEST_F(PinNamingDefaultTest, rejects_invalid_tiva_pins)
{
    EXPECT_EQ(std::nullopt, Parse(tiva, "PI0"));
    EXPECT_EQ(std::nullopt, Parse(tiva, "PF8"));
    EXPECT_EQ(std::nullopt, Parse(tiva, "PF01"));
    EXPECT_EQ(std::nullopt, Parse(tiva, "PF"));
    EXPECT_EQ(std::nullopt, Parse(tiva, "XF1"));
    EXPECT_EQ(std::nullopt, Parse(tiva, "PFa"));
    EXPECT_EQ(std::nullopt, Parse(tiva, ""));
}

TEST_F(PinNamingDefaultTest, parses_aliases_with_their_pull)
{
    EXPECT_EQ((services::hil::PinId{ 5, 1 }), Parse(tiva, "led"));
    EXPECT_EQ(services::hil::Pull::none, pull);
    EXPECT_EQ((services::hil::PinId{ 9, 0 }), Parse(tiva, "id0"));
    EXPECT_EQ(services::hil::Pull::up, pull);
    EXPECT_EQ(std::nullopt, Parse(tiva, "LED"));
}

TEST_F(PinNamingDefaultTest, parses_stm32_pins)
{
    EXPECT_EQ((services::hil::PinId{ 0, 15 }), Parse(stm32, "PA15"));
    EXPECT_EQ((services::hil::PinId{ 8, 0 }), Parse(stm32, "PI0"));
    EXPECT_EQ((services::hil::PinId{ 1, 10 }), Parse(stm32, "pb10"));
    EXPECT_EQ(std::nullopt, Parse(stm32, "PA16"));
    EXPECT_EQ(std::nullopt, Parse(stm32, "PA100"));
    EXPECT_EQ(std::nullopt, Parse(stm32, "PL0"));
}

TEST_F(PinNamingDefaultTest, placeholder_letter_skips_a_port)
{
    services::hil::PinNamingDefault gapped{ "AB-D", 7 };

    EXPECT_EQ((services::hil::PinId{ 3, 2 }), Parse(gapped, "PD2"));
    EXPECT_EQ(std::nullopt, Parse(gapped, "PC2"));
    EXPECT_EQ(std::nullopt, Parse(gapped, "P-2"));
}

TEST_F(PinNamingDefaultTest, prints_pins)
{
    EXPECT_EQ("PQ3", Print(tiva, services::hil::PinId{ 14, 3 }));
    EXPECT_EQ("PA15", Print(stm32, services::hil::PinId{ 0, 15 }));
}

TEST_F(PinNamingDefaultTest, exposes_aliases)
{
    EXPECT_EQ(2, tiva.Aliases().size());
    EXPECT_TRUE(stm32.Aliases().empty());
}
