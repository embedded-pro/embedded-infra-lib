#include "hal/qemu/cmsdk/CmsdkTimer.hpp"
#include "infra/util/Function.hpp"
#include "gtest/gtest.h"
#include <cstdint>
#include <optional>

namespace
{
    class CmsdkTimerTest
        : public testing::Test
    {
    public:
        void Construct(uint32_t frequencyHz)
        {
            timer.emplace(frequencyHz, onTick, reinterpret_cast<uintptr_t>(&registers), hal::cmsdkTimer0IrqNumber, hal::cmsdkTimerClockHz);
        }

        hal::cortex::InterruptTable::WithStorage<64> interruptTable;
        hal::CmsdkTimerRegisters registers{};
        uint32_t ticks{ 0 };
        infra::Function<void()> onTick{ [this]()
            {
                ++ticks;
            } };
        std::optional<hal::CmsdkTimer> timer;
    };
}

TEST_F(CmsdkTimerTest, construction_programs_the_reload_value_from_the_clock)
{
    Construct(20000);

    EXPECT_EQ(hal::cmsdkTimerClockHz / 20000u - 1u, registers.reload);
    EXPECT_EQ(hal::cmsdkTimerClockHz / 20000u - 1u, registers.value);
}

TEST_F(CmsdkTimerTest, construction_leaves_the_timer_stopped)
{
    registers.control = hal::timerControlEnable;

    Construct(20000);

    EXPECT_EQ(0u, registers.control);
}

TEST_F(CmsdkTimerTest, construction_registers_the_interrupt_handler)
{
    Construct(20000);

    EXPECT_NE(nullptr, interruptTable.Handler(hal::cmsdkTimer0IrqNumber));
}

TEST_F(CmsdkTimerTest, start_enables_the_timer_and_its_interrupt)
{
    Construct(20000);

    timer->Start();

    EXPECT_EQ(hal::timerControlEnable | hal::timerControlIntEnable, registers.control);
}

TEST_F(CmsdkTimerTest, stop_disables_the_timer)
{
    Construct(20000);
    timer->Start();

    timer->Stop();

    EXPECT_EQ(0u, registers.control);
}

TEST_F(CmsdkTimerTest, an_interrupt_clears_the_timer_interrupt_and_ticks)
{
    Construct(20000);
    timer->Start();

    interruptTable.Invoke(hal::cmsdkTimer0IrqNumber);

    EXPECT_EQ(hal::timerIntClear, registers.intClear);
    EXPECT_EQ(1u, ticks);
}

TEST_F(CmsdkTimerTest, destruction_stops_the_timer_and_unregisters_the_handler)
{
    Construct(20000);
    timer->Start();

    timer.reset();

    EXPECT_EQ(0u, registers.control);
    EXPECT_EQ(nullptr, interruptTable.Handler(hal::cmsdkTimer0IrqNumber));
}
