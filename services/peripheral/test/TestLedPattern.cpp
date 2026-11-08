#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/BoundedString.hpp"
#include "services/peripheral/LedPattern.hpp"
#include "gtest/gtest.h"

namespace
{
    infra::BoundedString::WithStorage<64> Render(const services::LedPattern& pattern, uint32_t ticks)
    {
        infra::BoundedString::WithStorage<64> levels;
        for (uint32_t tick = 0; tick != ticks; ++tick)
            levels.push_back(pattern.LevelAt(tick) ? '#' : '.');
        return levels;
    }

    class LedPatternPlayerTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        infra::BoundedString::WithStorage<64> Play(const services::LedPattern& pattern, uint32_t ticks)
        {
            infra::BoundedString::WithStorage<64> levels;
            for (uint32_t tick = 0; tick != ticks; ++tick)
            {
                player.Update(pattern);
                levels.push_back(led.GetOutputLatch() ? '#' : '.');
            }
            return levels;
        }

        hal::GpioPinSpy led;
        services::LedPatternPlayer player{ led };
    };
}

TEST(LedPatternTest, off_is_never_lit)
{
    EXPECT_EQ("........", Render(services::LedPattern::Off(), 8));
    EXPECT_EQ(1u, services::LedPattern::Off().PeriodTicks());
}

TEST(LedPatternTest, steady_is_always_lit)
{
    EXPECT_EQ("########", Render(services::LedPattern::Steady(), 8));
}

TEST(LedPatternTest, a_flash_group_is_followed_by_its_pause_and_repeats)
{
    const auto pattern = services::LedPattern::Flashes(2, 2, 1, 3);

    EXPECT_EQ(9u, pattern.PeriodTicks());
    EXPECT_EQ("##.##....##.##....", Render(pattern, 18));
}

TEST(LedPatternTest, a_single_flash_with_a_long_pause_is_a_heartbeat)
{
    const auto pattern = services::LedPattern::Flashes(1, 1, 0, 39);

    EXPECT_EQ(40u, pattern.PeriodTicks());
    EXPECT_TRUE(pattern.LevelAt(0));
    EXPECT_FALSE(pattern.LevelAt(1));
    EXPECT_FALSE(pattern.LevelAt(39));
    EXPECT_TRUE(pattern.LevelAt(40));
}

TEST(LedPatternTest, patterns_compare_by_value)
{
    EXPECT_EQ(services::LedPattern::Flashes(2, 1, 1, 3), services::LedPattern::Flashes(2, 1, 1, 3));
    EXPECT_FALSE(services::LedPattern::Flashes(2, 1, 1, 3) == services::LedPattern::Flashes(3, 1, 1, 3));
    EXPECT_FALSE(services::LedPattern::Off() == services::LedPattern::Steady());
    EXPECT_FALSE(services::LedPattern::Flashes(2, 1, 1, 3) == services::LedPattern::Flashes(2, 2, 1, 3));
    EXPECT_FALSE(services::LedPattern::Flashes(2, 1, 1, 3) == services::LedPattern::Flashes(2, 1, 2, 3));
    EXPECT_FALSE(services::LedPattern::Flashes(2, 1, 1, 3) == services::LedPattern::Flashes(2, 1, 1, 4));
}

TEST_F(LedPatternPlayerTest, the_led_follows_the_pattern_one_tick_per_update)
{
    EXPECT_EQ("#.#...#.#...", Play(services::LedPattern::Flashes(2, 1, 1, 2), 12));
}

TEST_F(LedPatternPlayerTest, an_unchanged_pattern_continues_where_it_was)
{
    const auto pattern = services::LedPattern::Flashes(1, 2, 0, 2);
    Play(pattern, 1);

    EXPECT_EQ("#..##.", Play(pattern, 6));
}

TEST_F(LedPatternPlayerTest, a_new_pattern_starts_from_its_beginning)
{
    Play(services::LedPattern::Flashes(1, 2, 0, 2), 3);

    EXPECT_EQ("#..", Play(services::LedPattern::Flashes(1, 1, 0, 2), 3));
}

TEST_F(LedPatternPlayerTest, the_first_update_drives_the_led_even_when_it_is_off)
{
    led.Set(true);

    player.Update(services::LedPattern::Off());

    EXPECT_FALSE(led.GetOutputLatch());
}
