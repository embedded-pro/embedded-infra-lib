#include "services/fsm/AlternativeId.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    struct Red
    {
        static constexpr const char* name{ "Red" };
    };

    struct Green
    {
        static constexpr const char* name{ "Green" };
        int intensity{ 0 };
    };

    struct Blue
    {
        static constexpr const char* name{ "Blue" };
    };

    using Colour = std::variant<Red, Green, Blue>;
    using ColourId = services::AlternativeId<Colour>;
}

TEST(AlternativeIdTest, of_returns_index_of_alternative)
{
    EXPECT_EQ(0, ColourId::Of<Red>().Index());
    EXPECT_EQ(1, ColourId::Of<Green>().Index());
    EXPECT_EQ(2, ColourId::Of<Blue>().Index());
}

TEST(AlternativeIdTest, of_value_returns_id_of_active_alternative)
{
    Colour colour{ Green{ 5 } };

    EXPECT_EQ(ColourId::Of<Green>(), ColourId::Of(colour));
}

TEST(AlternativeIdTest, from_index_constructs_id)
{
    EXPECT_EQ(ColourId::Of<Blue>(), ColourId::FromIndex(2));
}

TEST(AlternativeIdTest, is_matches_only_its_alternative)
{
    auto id = ColourId::Of<Green>();

    EXPECT_FALSE(id.Is<Red>());
    EXPECT_TRUE(id.Is<Green>());
    EXPECT_FALSE(id.Is<Blue>());
}

TEST(AlternativeIdTest, name_comes_from_alternative)
{
    EXPECT_STREQ("Red", ColourId::Of<Red>().Name());
    EXPECT_STREQ("Green", ColourId::Of<Green>().Name());
    EXPECT_STREQ("Blue", ColourId::Of<Blue>().Name());
}

TEST(AlternativeIdTest, ids_compare_by_index)
{
    EXPECT_EQ(ColourId::Of<Red>(), ColourId::Of<Red>());
    EXPECT_NE(ColourId::Of<Red>(), ColourId::Of<Blue>());
}

TEST(AlternativeIdTest, count_is_number_of_alternatives)
{
    static_assert(ColourId::count == 3);
    EXPECT_EQ(3, ColourId::count);
}

TEST(AlternativeIdTest, ids_are_usable_at_compile_time)
{
    static_assert(ColourId::Of<Blue>().Index() == 2);
    static_assert(ColourId::Of<Blue>().Is<Blue>());
    static_assert(!ColourId::Of<Blue>().Is<Red>());
}
