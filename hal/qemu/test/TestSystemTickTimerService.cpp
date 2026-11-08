#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/timer/Timer.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>

namespace
{
    constexpr uintptr_t systCsr = 0xE000E010;
    constexpr uint32_t csrEnable = 1u << 0;
    constexpr uintptr_t scbIcsr = 0xE000ED04;
    constexpr uint32_t icsrPendStClear = 1u << 25;

    bool SysTickCounting()
    {
        return (*reinterpret_cast<volatile uint32_t*>(systCsr) & csrEnable) != 0;
    }

    void HaltSysTick()
    {
        *reinterpret_cast<volatile uint32_t*>(systCsr) &= ~csrEnable;
        *reinterpret_cast<volatile uint32_t*>(scbIcsr) = icsrPendStClear;
    }

    class SystemTickTimerServiceHardwareTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        hal::cortex::InterruptTable::WithStorage<64> interruptTable;
        hal::cortex::SystemTickTimerService timerService{ 25000000u };
    };

    class SystemTickTimerServiceTest
        : public SystemTickTimerServiceHardwareTest
    {
    public:
        SystemTickTimerServiceTest()
        {
            HaltSysTick();
        }

        const infra::TimePoint start{ timerService.Now() };
    };
}

TEST_F(SystemTickTimerServiceTest, timer_fires_after_two_tick_interrupts)
{
    bool fired = false;
    infra::TimerSingleShot timer(std::chrono::milliseconds(1), [&fired]()
        {
            fired = true;
        });

    timerService.SystemTickInterrupt();
    ExecuteAllActions();
    timerService.SystemTickInterrupt();
    ExecuteAllActions();

    EXPECT_TRUE(fired);
}

TEST_F(SystemTickTimerServiceTest, time_advances_per_tick)
{
    timerService.SystemTickInterrupt();
    ExecuteAllActions();
    EXPECT_EQ(start + std::chrono::milliseconds(1), timerService.Now());
}

TEST_F(SystemTickTimerServiceTest, multiple_ticks_advance_time)
{
    for (int i = 0; i < 5; ++i)
    {
        timerService.SystemTickInterrupt();
        ExecuteAllActions();
    }
    EXPECT_EQ(start + std::chrono::milliseconds(5), timerService.Now());
}

TEST_F(SystemTickTimerServiceHardwareTest, construction_starts_the_hardware_timer)
{
    EXPECT_TRUE(SysTickCounting());
}
