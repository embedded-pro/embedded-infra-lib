#include "infra/stream/StringOutputStream.hpp"
#include "services/hil/HilPinNaming.hpp"
#include "gmock/gmock.h"
#include <array>

namespace
{
    const std::array<services::HilPinAlias, 2> aliases{ {
        { "led", { 5, 1 } },
        { "id0", { 9, 0 }, services::HilPull::up },
    } };
}

class HilPinNamingDefaultTest
    : public testing::Test
{
public:
    std::optional<services::HilPinId> Parse(const services::HilPinNaming& naming, infra::BoundedConstString text)
    {
        return naming.Parse(text, pull);
    }

    std::string Print(const services::HilPinNaming& naming, services::HilPinId pin)
    {
        infra::StringOutputStream::WithStorage<16> stream;
        naming.Print(stream, pin);
        return std::string(stream.Storage().begin(), stream.Storage().end());
    }

    services::HilPull pull = services::HilPull::down;
    services::HilPinNamingDefault tiva{ "ABCDEFGHJKLMNPQ", 7, infra::MakeRange(aliases) };
    services::HilPinNamingDefault stm32{ "ABCDEFGHIJK", 15 };
};

TEST_F(HilPinNamingDefaultTest, parses_tiva_pins)
{
    EXPECT_EQ((services::HilPinId{ 5, 1 }), Parse(tiva, "PF1"));
    EXPECT_EQ((services::HilPinId{ 8, 0 }), Parse(tiva, "PJ0"));
    EXPECT_EQ((services::HilPinId{ 14, 3 }), Parse(tiva, "pq3"));
    EXPECT_EQ(services::HilPull::down, pull);
}

TEST_F(HilPinNamingDefaultTest, rejects_invalid_tiva_pins)
{
    EXPECT_EQ(std::nullopt, Parse(tiva, "PI0"));
    EXPECT_EQ(std::nullopt, Parse(tiva, "PF8"));
    EXPECT_EQ(std::nullopt, Parse(tiva, "PF01"));
    EXPECT_EQ(std::nullopt, Parse(tiva, "PF"));
    EXPECT_EQ(std::nullopt, Parse(tiva, "XF1"));
    EXPECT_EQ(std::nullopt, Parse(tiva, "PFa"));
    EXPECT_EQ(std::nullopt, Parse(tiva, ""));
}

TEST_F(HilPinNamingDefaultTest, parses_aliases_with_their_pull)
{
    EXPECT_EQ((services::HilPinId{ 5, 1 }), Parse(tiva, "led"));
    EXPECT_EQ(services::HilPull::none, pull);
    EXPECT_EQ((services::HilPinId{ 9, 0 }), Parse(tiva, "id0"));
    EXPECT_EQ(services::HilPull::up, pull);
    EXPECT_EQ(std::nullopt, Parse(tiva, "LED"));
}

TEST_F(HilPinNamingDefaultTest, parses_stm32_pins)
{
    EXPECT_EQ((services::HilPinId{ 0, 15 }), Parse(stm32, "PA15"));
    EXPECT_EQ((services::HilPinId{ 8, 0 }), Parse(stm32, "PI0"));
    EXPECT_EQ((services::HilPinId{ 1, 10 }), Parse(stm32, "pb10"));
    EXPECT_EQ(std::nullopt, Parse(stm32, "PA16"));
    EXPECT_EQ(std::nullopt, Parse(stm32, "PA100"));
    EXPECT_EQ(std::nullopt, Parse(stm32, "PL0"));
}

TEST_F(HilPinNamingDefaultTest, placeholder_letter_skips_a_port)
{
    services::HilPinNamingDefault gapped{ "AB-D", 7 };

    EXPECT_EQ((services::HilPinId{ 3, 2 }), Parse(gapped, "PD2"));
    EXPECT_EQ(std::nullopt, Parse(gapped, "PC2"));
    EXPECT_EQ(std::nullopt, Parse(gapped, "P-2"));
}

TEST_F(HilPinNamingDefaultTest, prints_pins)
{
    EXPECT_EQ("PQ3", Print(tiva, services::HilPinId{ 14, 3 }));
    EXPECT_EQ("PA15", Print(stm32, services::HilPinId{ 0, 15 }));
}

TEST_F(HilPinNamingDefaultTest, exposes_aliases)
{
    EXPECT_EQ(2, tiva.Aliases().size());
    EXPECT_TRUE(stm32.Aliases().empty());
}
