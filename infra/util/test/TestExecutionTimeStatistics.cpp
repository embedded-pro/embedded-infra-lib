#include "infra/util/ExecutionTimeStatistics.hpp"
#include "gtest/gtest.h"
#include <cstdint>

namespace
{
    constexpr uint32_t budget = 4500;
    constexpr uint32_t period = 6000;

    class TestExecutionTimeStatistics
        : public ::testing::Test
    {
    public:
        TestExecutionTimeStatistics()
        {
            statistics.Configure(budget, period);
        }

        infra::ExecutionTimeStatistics statistics;
    };
}

TEST(SaturatingIncrementTest, increments_below_the_maximum)
{
    EXPECT_EQ(1u, infra::detail::SaturatingIncrement(0));
    EXPECT_EQ(UINT32_MAX, infra::detail::SaturatingIncrement(UINT32_MAX - 1));
}

TEST(SaturatingIncrementTest, stays_at_the_maximum)
{
    EXPECT_EQ(UINT32_MAX, infra::detail::SaturatingIncrement(UINT32_MAX));
}

TEST_F(TestExecutionTimeStatistics, an_unconfigured_instance_reports_nothing)
{
    infra::ExecutionTimeStatistics fresh;

    const auto snapshot = fresh.Read();

    EXPECT_EQ(0u, snapshot.samples);
    EXPECT_EQ(0u, snapshot.last);
    EXPECT_EQ(0u, snapshot.minimum);
    EXPECT_EQ(0u, snapshot.maximum);
    EXPECT_EQ(0u, snapshot.average);
    EXPECT_EQ(0u, snapshot.budget);
    EXPECT_EQ(0u, snapshot.overruns);
    EXPECT_EQ(0u, snapshot.deadlineMisses);
    EXPECT_EQ(0u, snapshot.reentries);
}

TEST_F(TestExecutionTimeStatistics, an_unconfigured_instance_measures_but_never_reports_an_overrun)
{
    infra::ExecutionTimeStatistics fresh;

    fresh.Record(UINT32_MAX - 1);

    const auto snapshot = fresh.Read();

    EXPECT_EQ(1u, snapshot.samples);
    EXPECT_EQ(UINT32_MAX - 1, snapshot.last);
    EXPECT_EQ(0u, snapshot.overruns);
    EXPECT_EQ(0u, snapshot.deadlineMisses);
    EXPECT_EQ(0u, snapshot.budget);
}

TEST_F(TestExecutionTimeStatistics, a_zero_budget_and_period_disable_the_thresholds_rather_than_tripping_them)
{
    infra::ExecutionTimeStatistics disabled;
    disabled.Configure(0, 0);

    disabled.Record(1);
    disabled.Record(UINT32_MAX - 1);

    const auto snapshot = disabled.Read();

    EXPECT_EQ(2u, snapshot.samples);
    EXPECT_EQ(0u, snapshot.overruns);
    EXPECT_EQ(0u, snapshot.deadlineMisses);
    EXPECT_EQ(0u, snapshot.budget);
}

TEST_F(TestExecutionTimeStatistics, a_zero_period_disables_only_the_deadline)
{
    infra::ExecutionTimeStatistics budgetOnly;
    budgetOnly.Configure(budget, 0);

    budgetOnly.Record(UINT32_MAX - 1);

    const auto snapshot = budgetOnly.Read();

    EXPECT_EQ(1u, snapshot.overruns);
    EXPECT_EQ(0u, snapshot.deadlineMisses);
    EXPECT_EQ(budget, snapshot.budget);
}

TEST_F(TestExecutionTimeStatistics, the_configured_budget_is_reported)
{
    EXPECT_EQ(budget, statistics.Read().budget);
}

TEST_F(TestExecutionTimeStatistics, the_first_sample_sets_every_extreme)
{
    statistics.Record(1200);

    const auto snapshot = statistics.Read();

    EXPECT_EQ(1u, snapshot.samples);
    EXPECT_EQ(1200u, snapshot.last);
    EXPECT_EQ(1200u, snapshot.minimum);
    EXPECT_EQ(1200u, snapshot.maximum);
    EXPECT_EQ(1200u, snapshot.average);
}

TEST_F(TestExecutionTimeStatistics, a_zero_duration_is_a_valid_minimum)
{
    statistics.Record(1200);
    statistics.Record(0);

    EXPECT_EQ(0u, statistics.Read().minimum);
}

TEST_F(TestExecutionTimeStatistics, the_extremes_track_the_run)
{
    statistics.Record(2000);
    statistics.Record(1000);
    statistics.Record(3000);

    const auto snapshot = statistics.Read();

    EXPECT_EQ(3u, snapshot.samples);
    EXPECT_EQ(3000u, snapshot.last);
    EXPECT_EQ(1000u, snapshot.minimum);
    EXPECT_EQ(3000u, snapshot.maximum);
}

TEST_F(TestExecutionTimeStatistics, the_average_settles_on_a_steady_duration)
{
    for (int sample = 0; sample != 200; ++sample)
        statistics.Record(1800);

    EXPECT_EQ(1800u, statistics.Read().average);
}

TEST_F(TestExecutionTimeStatistics, the_average_moves_a_sixteenth_of_the_way_toward_a_new_duration)
{
    for (int sample = 0; sample != 200; ++sample)
        statistics.Record(1000);

    statistics.Record(5000);

    EXPECT_EQ(1250u, statistics.Read().average);
}

TEST_F(TestExecutionTimeStatistics, exceeding_the_budget_counts_an_overrun_but_not_a_deadline_miss)
{
    statistics.Record(budget + 1);

    const auto snapshot = statistics.Read();

    EXPECT_EQ(1u, snapshot.overruns);
    EXPECT_EQ(0u, snapshot.deadlineMisses);
}

TEST_F(TestExecutionTimeStatistics, exceeding_the_period_counts_both)
{
    statistics.Record(period + 1);

    const auto snapshot = statistics.Read();

    EXPECT_EQ(1u, snapshot.overruns);
    EXPECT_EQ(1u, snapshot.deadlineMisses);
}

TEST_F(TestExecutionTimeStatistics, a_run_inside_the_budget_counts_neither)
{
    statistics.Record(budget);

    const auto snapshot = statistics.Read();

    EXPECT_EQ(0u, snapshot.overruns);
    EXPECT_EQ(0u, snapshot.deadlineMisses);
}

TEST_F(TestExecutionTimeStatistics, a_run_of_exactly_the_period_is_not_a_deadline_miss)
{
    statistics.Record(period);

    const auto snapshot = statistics.Read();

    EXPECT_EQ(1u, snapshot.overruns);
    EXPECT_EQ(0u, snapshot.deadlineMisses);
}

TEST_F(TestExecutionTimeStatistics, a_reentry_is_counted_separately_from_a_deadline_miss)
{
    statistics.RecordReentry();
    statistics.RecordReentry();

    const auto snapshot = statistics.Read();

    EXPECT_EQ(2u, snapshot.reentries);
    EXPECT_EQ(0u, snapshot.deadlineMisses);
    EXPECT_EQ(0u, snapshot.samples);
}

TEST_F(TestExecutionTimeStatistics, reset_clears_the_run_but_keeps_the_thresholds)
{
    statistics.Record(period + 1);
    statistics.RecordReentry();
    statistics.Reset();

    auto snapshot = statistics.Read();

    EXPECT_EQ(0u, snapshot.samples);
    EXPECT_EQ(0u, snapshot.last);
    EXPECT_EQ(0u, snapshot.minimum);
    EXPECT_EQ(0u, snapshot.maximum);
    EXPECT_EQ(0u, snapshot.average);
    EXPECT_EQ(0u, snapshot.overruns);
    EXPECT_EQ(0u, snapshot.deadlineMisses);
    EXPECT_EQ(0u, snapshot.reentries);
    EXPECT_EQ(budget, snapshot.budget);

    statistics.Record(period + 1);
    snapshot = statistics.Read();

    EXPECT_EQ(1u, snapshot.overruns);
    EXPECT_EQ(1u, snapshot.deadlineMisses);
}

TEST_F(TestExecutionTimeStatistics, reset_restarts_the_extremes_and_the_average)
{
    statistics.Record(100);
    statistics.Record(5000);
    statistics.Reset();

    statistics.Record(2000);

    const auto snapshot = statistics.Read();

    EXPECT_EQ(2000u, snapshot.minimum);
    EXPECT_EQ(2000u, snapshot.maximum);
    EXPECT_EQ(2000u, snapshot.average);
}

TEST_F(TestExecutionTimeStatistics, reconfiguring_replaces_the_thresholds_and_clears_the_run)
{
    statistics.Record(period + 1);
    statistics.RecordReentry();

    statistics.Configure(period + 10, period + 20);
    statistics.Record(period + 11);

    const auto snapshot = statistics.Read();

    EXPECT_EQ(1u, snapshot.samples);
    EXPECT_EQ(0u, snapshot.reentries);
    EXPECT_EQ(period + 10, snapshot.budget);
    EXPECT_EQ(1u, snapshot.overruns);
    EXPECT_EQ(0u, snapshot.deadlineMisses);
}

TEST_F(TestExecutionTimeStatistics, counters_keep_counting_well_past_a_16_bit_range)
{
    infra::ExecutionTimeStatistics counting;
    counting.Configure(1, 1);

    for (uint32_t sample = 0; sample != 70000; ++sample)
    {
        counting.Record(2);
        counting.RecordReentry();
    }

    const auto snapshot = counting.Read();

    EXPECT_EQ(70000u, snapshot.samples);
    EXPECT_EQ(70000u, snapshot.overruns);
    EXPECT_EQ(70000u, snapshot.deadlineMisses);
    EXPECT_EQ(70000u, snapshot.reentries);
}
