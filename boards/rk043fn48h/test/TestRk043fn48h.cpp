#include "boards/rk043fn48h/Rk043fn48h.hpp"
#include "gtest/gtest.h"
#include <cstdint>

namespace
{
    constexpr hal::DisplayTiming timing = boards::rk043fn48hTiming;

    uint32_t TotalWidth()
    {
        return timing.horizontalSync + timing.horizontalBackPorch + timing.active.width + timing.horizontalFrontPorch;
    }

    uint32_t TotalHeight()
    {
        return timing.verticalSync + timing.verticalBackPorch + timing.active.height + timing.verticalFrontPorch;
    }
}

TEST(Rk043fn48hTest, the_active_area_is_480_by_272)
{
    EXPECT_EQ((hal::DisplaySize{ 480, 272 }), timing.active);
}

TEST(Rk043fn48hTest, the_data_starts_43_clocks_after_the_start_of_hsync)
{
    EXPECT_EQ(43, timing.horizontalSync + timing.horizontalBackPorch);
}

TEST(Rk043fn48hTest, the_data_starts_12_lines_after_the_start_of_vsync)
{
    EXPECT_EQ(12, timing.verticalSync + timing.verticalBackPorch);
}

TEST(Rk043fn48hTest, the_synchronisation_and_the_data_enable_are_active_low_and_the_clock_is_not_inverted)
{
    EXPECT_FALSE(timing.hsyncActiveHigh);
    EXPECT_FALSE(timing.vsyncActiveHigh);
    EXPECT_FALSE(timing.dataEnableActiveHigh);
    EXPECT_FALSE(timing.pixelClockInverted);
}

TEST(Rk043fn48hTest, the_frame_rate_at_the_pixel_clock_is_about_60_hertz)
{
    const double framesPerSecond = static_cast<double>(timing.pixelClockHz) / (static_cast<double>(TotalWidth()) * TotalHeight());

    EXPECT_GT(framesPerSecond, 58.0);
    EXPECT_LT(framesPerSecond, 62.0);
}
