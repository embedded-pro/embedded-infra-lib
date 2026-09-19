#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "hal/qemu/async/WatchdogQemu.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    hal::CmsdkWatchdogRegisters& Peripheral()
    {
        return *reinterpret_cast<hal::CmsdkWatchdogRegisters*>(hal::cmsdkWatchdogBaseAddress);
    }
}

class WatchdogQemuTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    WatchdogQemuTest()
        : timerService(hal::cmsdkWatchdogClockHz)
    {
        config.timeout = std::chrono::milliseconds(50);
        config.feedTimerInterval = std::chrono::milliseconds(25);
        config.expirationTimeout = std::chrono::milliseconds(150);
        config.resetOnMissedInterrupt = false;
        config.base = reinterpret_cast<uintptr_t>(&registers);
    }

    void Timeout()
    {
        registers.ris = hal::watchdogRisTimeout;
        interruptTable.Invoke(hal::cortex::nmiIrq);
    }

    void ForwardTime(std::chrono::milliseconds duration)
    {
        for (auto elapsed = 0; elapsed != duration.count(); ++elapsed)
        {
            timerService.SystemTickInterrupt();
            ExecuteAllActions();
        }
    }

    // The repeating timer first triggers one resolution after its interval, so forward past it
    void ForwardPastAFeed()
    {
        ForwardTime(std::chrono::milliseconds(30));
    }

    hal::cortex::InterruptTable::WithStorage<64> interruptTable;
    hal::cortex::SystemTickTimerService timerService;
    hal::CmsdkWatchdogRegisters registers{};
    hal::WatchdogQemu::Config config;
    uint32_t expirations{ 0 };
    infra::Function<void()> onExpired{ [this]()
        {
            ++expirations;
        } };
};

TEST_F(WatchdogQemuTest, construction_programs_the_reload_value_from_the_clock)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    EXPECT_EQ(hal::cmsdkWatchdogClockHz / 20u, registers.load);
}

TEST_F(WatchdogQemuTest, construction_enables_the_interrupt)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    EXPECT_EQ(hal::watchdogControlIntEnable, registers.control);
}

TEST_F(WatchdogQemuTest, construction_registers_the_interrupt_handler_before_enabling_the_peripheral)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    EXPECT_NE(nullptr, interruptTable.Handler(hal::cortex::nmiIrq));
}

TEST_F(WatchdogQemuTest, construction_writes_the_unlock_key)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    EXPECT_EQ(hal::watchdogLockUnlockKey, registers.lock);
}

TEST_F(WatchdogQemuTest, reset_on_missed_interrupt_enables_the_reset_output)
{
    config.resetOnMissedInterrupt = true;
    hal::WatchdogQemu watchdog(onExpired, config);

    EXPECT_EQ(hal::watchdogControlIntEnable | hal::watchdogControlResetEnable, registers.control);
}

TEST_F(WatchdogQemuTest, a_reconstructed_watchdog_does_not_inherit_the_reset_output)
{
    {
        auto withReset = config;
        withReset.resetOnMissedInterrupt = true;
        hal::WatchdogQemu watchdog(onExpired, withReset);
    }

    hal::WatchdogQemu watchdog(onExpired, config);

    EXPECT_EQ(hal::watchdogControlIntEnable, registers.control);
}

TEST_F(WatchdogQemuTest, destruction_disables_the_watchdog)
{
    {
        hal::WatchdogQemu watchdog(onExpired, config);
    }

    EXPECT_EQ(0u, registers.control);
}

TEST_F(WatchdogQemuTest, a_timeout_clears_the_interrupt_so_the_hardware_does_not_reset)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    registers.intClr = 0xffffffffu;
    Timeout();

    EXPECT_EQ(0u, registers.intClr);
}

TEST_F(WatchdogQemuTest, an_interrupt_the_watchdog_did_not_raise_is_not_counted)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    for (uint32_t iteration = 0; iteration != 10; ++iteration)
        interruptTable.Invoke(hal::cortex::nmiIrq);

    EXPECT_EQ(0u, expirations);
}

TEST_F(WatchdogQemuTest, a_fed_watchdog_does_not_expire)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    for (uint32_t iteration = 0; iteration != 10; ++iteration)
    {
        Timeout();
        ForwardPastAFeed();
    }

    EXPECT_EQ(0u, expirations);
}

TEST_F(WatchdogQemuTest, consecutive_timeouts_without_a_feed_expire_the_watchdog)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    Timeout();
    Timeout();
    EXPECT_EQ(0u, expirations);

    Timeout();
    EXPECT_EQ(1u, expirations);
}

TEST_F(WatchdogQemuTest, a_feed_restarts_the_count_towards_expiry)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    Timeout();
    Timeout();

    ForwardPastAFeed();

    Timeout();
    Timeout();
    EXPECT_EQ(0u, expirations);

    Timeout();
    EXPECT_EQ(1u, expirations);
}

TEST_F(WatchdogQemuTest, expiry_is_reported_once_per_run_of_missed_feeds)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    for (uint32_t iteration = 0; iteration != 6; ++iteration)
        Timeout();

    EXPECT_EQ(1u, expirations);
}

TEST_F(WatchdogQemuTest, refresh_clears_a_raised_timeout)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    registers.intClr = 0xffffffffu;
    watchdog.Refresh();

    EXPECT_EQ(0u, registers.intClr);
}

class WatchdogQemuPeripheralTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    WatchdogQemuPeripheralTest()
        : timerService(hal::cmsdkWatchdogClockHz)
    {
        config.timeout = std::chrono::seconds(10);
        config.feedTimerInterval = std::chrono::seconds(1);
        config.expirationTimeout = std::chrono::seconds(10);
        config.resetOnMissedInterrupt = false;
    }

    hal::cortex::InterruptTable::WithStorage<64> interruptTable;
    hal::cortex::SystemTickTimerService timerService;
    hal::WatchdogQemu::Config config;
    infra::Function<void()> onExpired{ []() {} };
};

TEST_F(WatchdogQemuPeripheralTest, the_peripheral_accepts_the_programmed_reload_value)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    EXPECT_EQ(hal::cmsdkWatchdogClockHz * 10u, Peripheral().load);
    EXPECT_EQ(hal::watchdogControlIntEnable, Peripheral().control);
}

TEST_F(WatchdogQemuPeripheralTest, the_peripheral_counter_runs)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    auto first = Peripheral().value;
    auto moved = false;

    for (uint32_t attempt = 0; attempt != 1000000 && !moved; ++attempt)
        moved = Peripheral().value != first;

    EXPECT_TRUE(moved);
    EXPECT_LT(Peripheral().value, first);
}

TEST_F(WatchdogQemuPeripheralTest, the_peripheral_reports_its_registers_as_unlocked)
{
    hal::WatchdogQemu watchdog(onExpired, config);

    EXPECT_EQ(0u, Peripheral().lock);
}

TEST_F(WatchdogQemuPeripheralTest, destruction_leaves_the_peripheral_disabled)
{
    {
        hal::WatchdogQemu watchdog(onExpired, config);
    }

    EXPECT_EQ(0u, Peripheral().control);
}
