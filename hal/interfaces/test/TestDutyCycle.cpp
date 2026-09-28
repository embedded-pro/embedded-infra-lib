#include "hal/interfaces/DutyCycle.hpp"
#include "gtest/gtest.h"

TEST(DutyCycleTest, full_scale_is_one_hundred_percent)
{
    EXPECT_EQ(hal::DutyCycle::FromPercent(100).Value(), hal::DutyCycle::fullScale);
    EXPECT_EQ(hal::DutyCycle::FromPercent(50).Value(), hal::DutyCycle::fullScale / 2);
    EXPECT_EQ(hal::DutyCycle::FromPercent(0).Value(), 0u);
}

TEST(DutyCycleTest, from_percent_rounds_to_the_nearest_step)
{
    EXPECT_EQ(hal::DutyCycle::FromPercent(1).Value(), 655u);
    EXPECT_EQ(hal::DutyCycle::FromPercent(15).Value(), 9830u);
}

TEST(DutyCycleTest, from_ratio_scales_to_full_scale)
{
    EXPECT_EQ(hal::DutyCycle::FromRatio(1, 1).Value(), hal::DutyCycle::fullScale);
    EXPECT_EQ(hal::DutyCycle::FromRatio(1, 4).Value(), hal::DutyCycle::fullScale / 4);
    EXPECT_EQ(hal::DutyCycle::FromRatio(0, 7).Value(), 0u);
}

TEST(DutyCycleTest, from_ratio_rounds_to_the_nearest_step)
{
    EXPECT_EQ(hal::DutyCycle::FromRatio(1, 3).Value(), 21845u);
    EXPECT_EQ(hal::DutyCycle::FromRatio(2, 3).Value(), 43691u);
    EXPECT_EQ(hal::DutyCycle::FromRatio(123456, 1000000).Value(), 8091u);
}

TEST(DutyCycleTest, from_percent_matches_from_ratio)
{
    for (uint32_t percent = 0; percent <= 100; ++percent)
        EXPECT_EQ(hal::DutyCycle::FromPercent(percent), hal::DutyCycle::FromRatio(percent, 100));
}

TEST(DutyCycleTest, to_counts_scales_the_period)
{
    EXPECT_EQ(hal::DutyCycle::FromPercent(50).ToCounts(3000), 1500u);
    EXPECT_EQ(hal::DutyCycle::FromPercent(100).ToCounts(3000), 3000u);
    EXPECT_EQ(hal::DutyCycle::FromPercent(0).ToCounts(3000), 0u);
}

TEST(DutyCycleTest, to_counts_rounds_to_the_nearest_count)
{
    EXPECT_EQ(hal::DutyCycle(hal::DutyCycle::fullScale / 3).ToCounts(3), 1u);
    EXPECT_EQ(hal::DutyCycle(hal::DutyCycle::fullScale * 5 / 6 + 1).ToCounts(3), 3u);
}

TEST(DutyCycleTest, to_counts_resolves_below_one_percent)
{
    const hal::DutyCycle duty{ hal::DutyCycle::fullScale / 1000 };

    EXPECT_EQ(duty.ToCounts(3000), 3u);
}

TEST(DutyCycleTest, to_counts_does_not_overflow_on_a_full_32_bit_period)
{
    constexpr uint64_t period = uint64_t{ 1 } << 32;

    EXPECT_EQ(hal::DutyCycle::FromPercent(100).ToCounts(period), period);
    EXPECT_EQ(hal::DutyCycle::FromPercent(50).ToCounts(period), period / 2);
}

TEST(DutyCycleTest, only_up_to_full_scale_is_valid)
{
    EXPECT_TRUE(hal::DutyCycle(hal::DutyCycle::fullScale).IsValid());
    EXPECT_FALSE(hal::DutyCycle(hal::DutyCycle::fullScale + 1).IsValid());
}
