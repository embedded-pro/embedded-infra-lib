#include "hal/cortex_m/DataWatchpointAndTrace.hpp"
#include "gtest/gtest.h"

TEST(DataWatchpointAndTraceTest, cycles_are_zero_after_start)
{
    hal::cortex::DataWatchpointAndTrace dwt;
    dwt.Start();

    EXPECT_EQ(0u, dwt.Cycles());
}

TEST(DataWatchpointAndTraceTest, cycles_are_zero_after_second_start)
{
    hal::cortex::DataWatchpointAndTrace dwt;
    dwt.Start();
    dwt.Stop();
    dwt.Start();

    EXPECT_EQ(0u, dwt.Cycles());
}
