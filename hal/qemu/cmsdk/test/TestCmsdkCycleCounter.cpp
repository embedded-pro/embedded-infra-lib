#include "hal/qemu/cmsdk/CmsdkCycleCounter.hpp"
#include "gtest/gtest.h"
#include <cstdint>
#include <optional>

namespace
{
    class CmsdkCycleCounterTest
        : public testing::Test
    {
    public:
        hal::CmsdkTimerRegisters registers{};
        std::optional<hal::CmsdkCycleCounter> counter{ std::in_place, reinterpret_cast<uintptr_t>(&registers) };
    };
}

TEST_F(CmsdkCycleCounterTest, construction_loads_the_full_scale_count)
{
    EXPECT_EQ(0xffffffffu, registers.reload);
    EXPECT_EQ(0xffffffffu, registers.value);
}

TEST_F(CmsdkCycleCounterTest, construction_starts_the_timer_without_its_interrupt)
{
    EXPECT_EQ(hal::timerControlEnable, registers.control);
}

TEST_F(CmsdkCycleCounterTest, now_is_zero_right_after_construction)
{
    EXPECT_EQ(0u, counter->Now());
}

TEST_F(CmsdkCycleCounterTest, now_counts_up_as_the_timer_counts_down)
{
    registers.value = 0xffffffffu - 1234u;

    EXPECT_EQ(1234u, counter->Now());
}

TEST_F(CmsdkCycleCounterTest, the_difference_of_two_readings_survives_a_wrap)
{
    registers.value = 10u;
    const auto before = counter->Now();
    registers.value = 0xffffffffu - 5u;

    EXPECT_EQ(16u, counter->Now() - before);
}

TEST_F(CmsdkCycleCounterTest, destruction_stops_the_timer)
{
    counter.reset();

    EXPECT_EQ(0u, registers.control);
}
