#include "services/hil/commands/HilPendingOperation.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

class HilPendingOperationTest
    : public services::HilFixture
{
public:
    services::HilPendingOperation operation{ response };
};

TEST_F(HilPendingOperationTest, is_busy_until_completed)
{
    EXPECT_FALSE(operation.Busy());

    auto current = operation.Start(std::chrono::seconds(1));
    EXPECT_TRUE(operation.Busy());

    EXPECT_TRUE(operation.Complete(current));
    EXPECT_FALSE(operation.Busy());

    ForwardTime(std::chrono::seconds(1));
    EXPECT_EQ("", Output());
}

TEST_F(HilPendingOperationTest, timeout_reports_an_error_and_stays_busy_until_completed)
{
    auto current = operation.Start(std::chrono::seconds(1));

    ForwardTime(std::chrono::seconds(1));
    EXPECT_EQ("\r\nERR timeout\r\n", Output());
    EXPECT_TRUE(operation.Busy());

    EXPECT_FALSE(operation.Complete(current));
    EXPECT_FALSE(operation.Busy());
}

TEST_F(HilPendingOperationTest, cancel_drops_the_completion_and_the_timeout)
{
    auto current = operation.Start(std::chrono::seconds(1));

    operation.Cancel();
    EXPECT_FALSE(operation.Busy());

    ForwardTime(std::chrono::seconds(1));
    EXPECT_EQ("", Output());
    EXPECT_FALSE(operation.Complete(current));
}

TEST_F(HilPendingOperationTest, stale_completion_does_not_finish_a_newer_operation)
{
    auto stale = operation.Start(std::chrono::seconds(1));
    operation.Cancel();
    auto current = operation.Start(std::chrono::seconds(1));

    EXPECT_FALSE(operation.Complete(stale));
    EXPECT_TRUE(operation.Busy());

    EXPECT_TRUE(operation.Complete(current));
    EXPECT_FALSE(operation.Busy());
}
