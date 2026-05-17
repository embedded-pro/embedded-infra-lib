#include "hal/interfaces/AnalogComparator.hpp"
#include "hal/interfaces/test_doubles/AnalogComparatorMock.hpp"
#include "gtest/gtest.h"

TEST(AnalogComparatorTest, EnableRegistersCallback)
{
    testing::StrictMock<hal::AnalogComparatorMock> comparator;

    EXPECT_CALL(comparator, Enable(testing::_, hal::InterruptTrigger::risingEdge));
    comparator.Enable([](bool) {}, hal::InterruptTrigger::risingEdge);
}

TEST(AnalogComparatorTest, DisableStopsComparator)
{
    testing::StrictMock<hal::AnalogComparatorMock> comparator;

    EXPECT_CALL(comparator, Disable());
    comparator.Disable();
}

TEST(AnalogComparatorTest, GetOutputReturnsFalse_WhenNegativeExceedsPositive)
{
    testing::StrictMock<hal::AnalogComparatorMock> comparator;

    EXPECT_CALL(comparator, GetOutput()).WillOnce(testing::Return(false));
    EXPECT_FALSE(comparator.GetOutput());
}

TEST(AnalogComparatorTest, GetOutputReturnsTrue_WhenPositiveExceedsNegative)
{
    testing::StrictMock<hal::AnalogComparatorMock> comparator;

    EXPECT_CALL(comparator, GetOutput()).WillOnce(testing::Return(true));
    EXPECT_TRUE(comparator.GetOutput());
}

TEST(AnalogComparatorTest, EnableWithBothEdgesTrigger)
{
    testing::StrictMock<hal::AnalogComparatorMock> comparator;

    EXPECT_CALL(comparator, Enable(testing::_, hal::InterruptTrigger::bothEdges));
    comparator.Enable([](bool) {}, hal::InterruptTrigger::bothEdges);
}
