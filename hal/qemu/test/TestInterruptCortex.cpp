#include "hal/cortex_m/InterruptCortex.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    constexpr int32_t unwiredIrq = 30;
    constexpr int32_t otherUnwiredIrq = 31;

    class CountingInterruptHandler
        : public hal::cortex::InterruptHandler
    {
    public:
        void Invoke() override
        {
            ++invocations;
        }

        uint32_t invocations{ 0 };
    };
}

class InterruptCortexTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    hal::cortex::InterruptTable::WithStorage<64> interruptTable;
};

TEST_F(InterruptCortexTest, invoke_routes_to_the_registered_handler)
{
    CountingInterruptHandler handler;
    handler.Register(unwiredIrq);

    interruptTable.Invoke(unwiredIrq);

    EXPECT_EQ(1u, handler.invocations);
}

TEST_F(InterruptCortexTest, register_stores_the_handler_in_the_table)
{
    CountingInterruptHandler handler;
    handler.Register(unwiredIrq);

    EXPECT_EQ(&handler, interruptTable.Handler(unwiredIrq));
    EXPECT_TRUE(handler.Registered());
    EXPECT_EQ(unwiredIrq, handler.Irq());
}

TEST_F(InterruptCortexTest, register_stores_the_requested_priority)
{
    CountingInterruptHandler handler;
    handler.Register(unwiredIrq, hal::cortex::InterruptPriority::high);

    EXPECT_EQ(hal::cortex::InterruptPriority::high, handler.Priority());
}

TEST_F(InterruptCortexTest, destructor_unregisters_the_handler)
{
    {
        CountingInterruptHandler handler;
        handler.Register(unwiredIrq);
        ASSERT_EQ(&handler, interruptTable.Handler(unwiredIrq));
    }

    EXPECT_EQ(nullptr, interruptTable.Handler(unwiredIrq));
}

TEST_F(InterruptCortexTest, unregister_clears_the_slot)
{
    CountingInterruptHandler handler;
    handler.Register(unwiredIrq);

    handler.Unregister();

    EXPECT_EQ(nullptr, interruptTable.Handler(unwiredIrq));
    EXPECT_FALSE(handler.Registered());
}

TEST_F(InterruptCortexTest, the_slot_is_free_for_reuse_after_unregistering)
{
    CountingInterruptHandler first;
    first.Register(unwiredIrq);
    first.Unregister();

    CountingInterruptHandler second;
    second.Register(unwiredIrq);

    EXPECT_EQ(&second, interruptTable.Handler(unwiredIrq));
}

TEST_F(InterruptCortexTest, move_construction_transfers_registration)
{
    CountingInterruptHandler original;
    original.Register(unwiredIrq);

    CountingInterruptHandler moved(std::move(original));

    EXPECT_EQ(&moved, interruptTable.Handler(unwiredIrq));
    EXPECT_TRUE(moved.Registered());
    EXPECT_FALSE(original.Registered());
}

TEST_F(InterruptCortexTest, the_moved_to_handler_receives_subsequent_invocations)
{
    CountingInterruptHandler original;
    original.Register(unwiredIrq);
    CountingInterruptHandler moved(std::move(original));

    interruptTable.Invoke(unwiredIrq);

    EXPECT_EQ(1u, moved.invocations);
    EXPECT_EQ(0u, original.invocations);
}

TEST_F(InterruptCortexTest, move_assignment_unregisters_the_previous_registration)
{
    CountingInterruptHandler source;
    source.Register(unwiredIrq);
    CountingInterruptHandler target;
    target.Register(otherUnwiredIrq);

    target = std::move(source);

    EXPECT_EQ(nullptr, interruptTable.Handler(otherUnwiredIrq));
    EXPECT_EQ(&target, interruptTable.Handler(unwiredIrq));
    EXPECT_FALSE(source.Registered());
}

TEST_F(InterruptCortexTest, dispatched_handler_defers_the_callback_to_the_event_dispatcher)
{
    bool invoked = false;
    hal::cortex::DispatchedInterruptHandler handler(unwiredIrq, [&invoked]()
        {
            invoked = true;
        });

    interruptTable.Invoke(unwiredIrq);
    EXPECT_FALSE(invoked);

    ExecuteAllActions();
    EXPECT_TRUE(invoked);
}

TEST_F(InterruptCortexTest, dispatched_handler_accepts_a_second_interrupt_after_the_callback_completed)
{
    uint32_t invocations = 0;
    hal::cortex::DispatchedInterruptHandler handler(unwiredIrq, [&invocations]()
        {
            ++invocations;
        });

    interruptTable.Invoke(unwiredIrq);
    ExecuteAllActions();
    interruptTable.Invoke(unwiredIrq);
    ExecuteAllActions();

    EXPECT_EQ(2u, invocations);
}

TEST_F(InterruptCortexTest, set_invoke_replaces_the_dispatched_callback)
{
    bool original = false;
    bool replacement = false;
    hal::cortex::DispatchedInterruptHandler handler(unwiredIrq, [&original]()
        {
            original = true;
        });

    handler.SetInvoke([&replacement]()
        {
            replacement = true;
        });
    interruptTable.Invoke(unwiredIrq);
    ExecuteAllActions();

    EXPECT_FALSE(original);
    EXPECT_TRUE(replacement);
}

TEST_F(InterruptCortexTest, dispatched_handler_does_not_run_the_callback_after_being_unregistered)
{
    bool invoked = false;
    hal::cortex::DispatchedInterruptHandler handler(unwiredIrq, [&invoked]()
        {
            invoked = true;
        });

    interruptTable.Invoke(unwiredIrq);
    handler.Unregister();
    ExecuteAllActions();

    EXPECT_FALSE(invoked);
}

TEST_F(InterruptCortexTest, immediate_handler_runs_the_callback_in_interrupt_context)
{
    bool invoked = false;
    hal::cortex::ImmediateInterruptHandler handler(unwiredIrq, [&invoked]()
        {
            invoked = true;
        });

    interruptTable.Invoke(unwiredIrq);

    EXPECT_TRUE(invoked);
}
