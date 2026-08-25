#include "hal/cortex_m/LowPriorityInterrupt.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

class LowPriorityInterruptTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    hal::cortex::InterruptTable::WithStorage<64> interruptTable;
    hal::cortex::LowPriorityInterrupt lpi;
};

TEST_F(LowPriorityInterruptTest, register_stores_handler_in_table)
{
    lpi.Register([]() {});

    EXPECT_NE(nullptr, interruptTable.Handler(hal::cortex::pendSvIrq));
}

TEST_F(LowPriorityInterruptTest, invoke_calls_registered_callback)
{
    bool called{ false };
    lpi.Register([&called]()
        {
            called = true;
        });

    interruptTable.Invoke(hal::cortex::pendSvIrq);

    EXPECT_TRUE(called);
}

TEST_F(LowPriorityInterruptTest, unregister_clears_the_slot)
{
    lpi.Register([]() {});
    lpi.Unregister();

    EXPECT_EQ(nullptr, interruptTable.Handler(hal::cortex::pendSvIrq));
}

TEST_F(LowPriorityInterruptTest, invoke_does_not_call_callback_after_unregister)
{
    bool called{ false };
    lpi.Register([&called]()
        {
            called = true;
        });
    lpi.Unregister();

    EXPECT_EQ(nullptr, interruptTable.Handler(hal::cortex::pendSvIrq));
    EXPECT_FALSE(called);
}
