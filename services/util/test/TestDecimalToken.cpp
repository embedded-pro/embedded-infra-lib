#include "services/util/DecimalToken.hpp"
#include "gtest/gtest.h"
#include <cstdint>

TEST(DecimalTokenTest, a_token_is_digits_with_an_optional_sign_and_fraction)
{
    EXPECT_TRUE(services::IsDecimalToken("12"));
    EXPECT_TRUE(services::IsDecimalToken("-1.5"));
    EXPECT_FALSE(services::IsDecimalToken("+12"));
    EXPECT_FALSE(services::IsDecimalToken("1e3"));
    EXPECT_FALSE(services::IsDecimalToken("2."));
    EXPECT_FALSE(services::IsDecimalToken("-"));
    EXPECT_FALSE(services::IsDecimalToken(""));
}

TEST(DecimalTokenTest, sign_and_fraction_can_be_refused)
{
    EXPECT_FALSE(services::IsDecimalToken("-12", false, true));
    EXPECT_FALSE(services::IsDecimalToken("1.5", true, false));
}

TEST(DecimalTokenTest, a_token_is_at_most_nine_integer_and_six_fraction_digits)
{
    EXPECT_TRUE(services::IsDecimalToken("-999999999.999999"));
    EXPECT_FALSE(services::IsDecimalToken("1000000000"));
    EXPECT_FALSE(services::IsDecimalToken("0.1234567"));
}

TEST(DecimalTokenTest, a_float_is_parsed_from_a_whole_or_decimal_number)
{
    EXPECT_EQ(3.0f, services::ParseDecimal("3", -10.0f, 10.0f));
    EXPECT_EQ(0.0f, services::ParseDecimal("0", -10.0f, 10.0f));
    EXPECT_EQ(-12.0f, services::ParseDecimal("-12", -20.0f, 20.0f));
    EXPECT_EQ(2.5f, services::ParseDecimal("2.5", -10.0f, 10.0f));
}

TEST(DecimalTokenTest, a_value_outside_the_bounds_is_rejected)
{
    EXPECT_FALSE(services::ParseDecimal("30", 1.0f, 20.0f).has_value());
    EXPECT_EQ(20.0f, services::ParseDecimal("20", 1.0f, 20.0f));
    EXPECT_FALSE(services::ParseDecimal<uint8_t>("100", 1, 99).has_value());
}

TEST(DecimalTokenTest, text_and_trailing_characters_are_rejected)
{
    EXPECT_FALSE(services::ParseDecimal("abc", -10.0f, 10.0f).has_value());
    EXPECT_FALSE(services::ParseDecimal("5abc", -10.0f, 10.0f).has_value());
    EXPECT_FALSE(services::ParseDecimal("2.5x", -10.0f, 10.0f).has_value());
}

TEST(DecimalTokenTest, an_integer_is_parsed_without_sign_or_fraction_for_an_unsigned_type)
{
    EXPECT_EQ(42u, services::ParseDecimal<uint32_t>("42", 0, 100));
    EXPECT_FALSE(services::ParseDecimal<uint32_t>("-1", 0, 100).has_value());
    EXPECT_FALSE(services::ParseDecimal<uint32_t>("1.5", 0, 100).has_value());
}

TEST(DecimalTokenTest, a_signed_integer_accepts_a_sign)
{
    EXPECT_EQ(-7, services::ParseDecimal<int32_t>("-7", -10, 10));
}

TEST(DecimalTokenTest, the_largest_nine_digit_integer_is_parsed)
{
    EXPECT_EQ(999999999u, services::ParseDecimal<uint32_t>("999999999", 0, 0xFFFFFFFF));
}

TEST(DecimalTokenTest, each_type_rejects_a_bad_token_and_values_outside_both_bounds)
{
    EXPECT_FALSE(services::ParseDecimal<uint8_t>("abc", 1, 99).has_value());
    EXPECT_FALSE(services::ParseDecimal<uint8_t>("0", 1, 99).has_value());
    EXPECT_EQ(50, services::ParseDecimal<uint8_t>("50", 1, 99));

    EXPECT_FALSE(services::ParseDecimal<uint32_t>("5", 10, 100).has_value());
    EXPECT_FALSE(services::ParseDecimal<uint32_t>("200", 10, 100).has_value());

    EXPECT_FALSE(services::ParseDecimal<int32_t>("1.5", -10, 10).has_value());
    EXPECT_FALSE(services::ParseDecimal<int32_t>("-20", -10, 10).has_value());
    EXPECT_FALSE(services::ParseDecimal<int32_t>("20", -10, 10).has_value());

    EXPECT_FALSE(services::ParseDecimal("-30", 1.0f, 20.0f).has_value());
}
