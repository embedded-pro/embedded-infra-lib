#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/LowPowerModeCortex.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    constexpr int32_t unwiredIrq = 30;
    constexpr uintptr_t nvicIspr0 = 0xE000E200u;
    constexpr uintptr_t scbScr = 0xE000ED10u;
    constexpr uint32_t scrSleepDeep = 1u << 2;

    volatile uint32_t& Register(uintptr_t address)
    {
        return *reinterpret_cast<volatile uint32_t*>(address);
    }

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

class LowPowerModeCortexTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    LowPowerModeCortexTest()
    {
        handler.Register(unwiredIrq);
    }

    void EnterWithPendingInterrupt(hal::PowerMode mode)
    {
        __asm volatile("cpsid i" ::: "memory");
        Register(nvicIspr0) = 1u << unwiredIrq;
        lowPowerMode.Enter(mode);
        __asm volatile("cpsie i" ::: "memory");
    }

    hal::cortex::InterruptTable::WithStorage<64> interruptTable;
    CountingInterruptHandler handler;
    hal::cortex::LowPowerModeCortex lowPowerMode;
};

TEST_F(LowPowerModeCortexTest, sleep_returns_on_a_pending_interrupt_which_runs_once_unmasked)
{
    EnterWithPendingInterrupt(hal::PowerMode::sleep);

    EXPECT_EQ(1u, handler.invocations);
}

TEST_F(LowPowerModeCortexTest, deep_sleep_falls_back_to_sleep)
{
    EnterWithPendingInterrupt(hal::PowerMode::deepSleep);

    EXPECT_EQ(1u, handler.invocations);
    EXPECT_EQ(0u, Register(scbScr) & scrSleepDeep);
}

TEST_F(LowPowerModeCortexTest, wait_for_interrupt_leaves_sleep_deep_cleared)
{
    __asm volatile("cpsid i" ::: "memory");
    Register(nvicIspr0) = 1u << unwiredIrq;
    hal::cortex::WaitForInterrupt(true);
    __asm volatile("cpsie i" ::: "memory");

    EXPECT_EQ(1u, handler.invocations);
    EXPECT_EQ(0u, Register(scbScr) & scrSleepDeep);
}
