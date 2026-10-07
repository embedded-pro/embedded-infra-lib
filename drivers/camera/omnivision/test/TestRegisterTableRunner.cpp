#include "drivers/camera/omnivision/RegisterTable.hpp"
#include "drivers/camera/omnivision/RegisterTableRunner.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/util/test_doubles/RegisterBusAccessMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace
{
    class RegisterTableRunnerTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        testing::StrictMock<services::RegisterBusAccessMock> bus;
        drivers::RegisterTableRunner runner{ bus };
        testing::StrictMock<infra::MockCallback<void()>> done;
    };

    class RegisterTableRunnerSlowBusTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        RegisterTableRunnerSlowBusTest()
        {
            bus.completeAutomatically = false;
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        drivers::RegisterTableRunner runner{ bus };
        testing::StrictMock<infra::MockCallback<void()>> done;
    };

    constexpr std::array<drivers::RegisterStep, 3> threeWrites{ {
        drivers::RegisterStep::Write(0x10, 0xaa),
        drivers::RegisterStep::Write(0x20, 0xbb),
        drivers::RegisterStep::Write(0x30, 0xcc),
    } };
}

TEST_F(RegisterTableRunnerTest, writes_are_issued_in_table_order)
{
    testing::InSequence seq;
    EXPECT_CALL(bus, WriteRegisterMock(0x10, std::vector<uint8_t>{ 0xaa }));
    EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0xbb }));
    EXPECT_CALL(bus, WriteRegisterMock(0x30, std::vector<uint8_t>{ 0xcc }));
    EXPECT_CALL(done, callback());

    runner.Run(infra::MakeRange(threeWrites), [this]()
        {
            done.callback();
        });

    ExecuteAllActions();
}

TEST_F(RegisterTableRunnerTest, completion_is_reported_after_the_last_write)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x10, std::vector<uint8_t>{ 0xaa }));
    EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0xbb }));
    EXPECT_CALL(bus, WriteRegisterMock(0x30, std::vector<uint8_t>{ 0xcc }));
    EXPECT_CALL(done, callback());

    runner.Run(infra::MakeRange(threeWrites), [this]()
        {
            done.callback();
        });

    ExecuteAllActions();
}

TEST_F(RegisterTableRunnerTest, an_empty_table_completes_asynchronously)
{
    bool called{ false };
    runner.Run(infra::MemoryRange<const drivers::RegisterStep>{}, [&called]()
        {
            called = true;
        });

    EXPECT_FALSE(called);

    ExecuteAllActions();

    EXPECT_TRUE(called);
}

TEST_F(RegisterTableRunnerTest, completion_is_never_reported_from_within_Run)
{
    bool calledDuringRun{ false };

    runner.Run(infra::MemoryRange<const drivers::RegisterStep>{}, [&calledDuringRun]()
        {
            calledDuringRun = true;
        });

    EXPECT_FALSE(calledDuringRun);
    ExecuteAllActions();
}

TEST_F(RegisterTableRunnerTest, a_delay_step_waits_before_the_next_write)
{
    constexpr std::array<drivers::RegisterStep, 2> table{ {
        drivers::RegisterStep::DelayMilliseconds(50),
        drivers::RegisterStep::Write(0x11, 0x77),
    } };

    EXPECT_CALL(done, callback());

    runner.Run(infra::MakeRange(table), [this]()
        {
            done.callback();
        });

    ExecuteAllActions();
    EXPECT_CALL(bus, WriteRegisterMock(0x11, std::vector<uint8_t>{ 0x77 }));
    ForwardTime(std::chrono::milliseconds(50));
    ExecuteAllActions();
}

TEST_F(RegisterTableRunnerTest, a_modify_step_reads_then_writes_with_the_masks_applied)
{
    constexpr std::array<drivers::RegisterStep, 1> table{ {
        drivers::RegisterStep::Modify(0x15, 0xf0, 0x0a),
    } };

    testing::InSequence seq;
    EXPECT_CALL(bus, ReadRegisterMock(0x15, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0b11001100 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x15, std::vector<uint8_t>{ 0b00001110 }));
    EXPECT_CALL(done, callback());

    runner.Run(infra::MakeRange(table), [this]()
        {
            done.callback();
        });

    ExecuteAllActions();
}

TEST_F(RegisterTableRunnerTest, a_modify_step_preserves_bits_outside_the_masks)
{
    constexpr std::array<drivers::RegisterStep, 1> table{ {
        drivers::RegisterStep::Modify(0x15, 0xf0, 0x03),
    } };

    testing::InSequence seq;
    EXPECT_CALL(bus, ReadRegisterMock(0x15, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0b10101111 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x15, std::vector<uint8_t>{ 0b00001111 }));
    EXPECT_CALL(done, callback());

    runner.Run(infra::MakeRange(table), [this]()
        {
            done.callback();
        });

    ExecuteAllActions();
}

TEST_F(RegisterTableRunnerSlowBusTest, a_slow_bus_holds_the_next_step_until_completion)
{
    constexpr std::array<drivers::RegisterStep, 2> table{ {
        drivers::RegisterStep::Write(0x01, 0x11),
        drivers::RegisterStep::Write(0x02, 0x22),
    } };

    EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x11 }));
    runner.Run(infra::MakeRange(table), [this]()
        {
            done.callback();
        });

    EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x22 }));
    EXPECT_CALL(done, callback());
    bus.CompletePending();
    bus.CompletePending();
}

TEST_F(RegisterTableRunnerTest, a_table_of_three_hundred_steps_runs_in_one_pass)
{
    constexpr std::size_t count = 300;
    std::array<drivers::RegisterStep, count> table{};
    for (std::size_t i = 0; i < count; ++i)
        table[i] = drivers::RegisterStep::Write(static_cast<uint8_t>(i & 0xff), static_cast<uint8_t>(i & 0xff));

    EXPECT_CALL(bus, WriteRegisterMock(testing::_, testing::_)).Times(static_cast<int>(count));
    EXPECT_CALL(done, callback());

    runner.Run(infra::MakeRange(table), [this]()
        {
            done.callback();
        });

    ExecuteAllActions();
}

TEST_F(RegisterTableRunnerTest, Busy_is_true_until_the_completion_has_been_delivered)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x10, std::vector<uint8_t>{ 0xaa }));
    EXPECT_CALL(bus, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0xbb }));
    EXPECT_CALL(bus, WriteRegisterMock(0x30, std::vector<uint8_t>{ 0xcc }));

    bool busyDuringCallback{ false };
    runner.Run(infra::MakeRange(threeWrites), [this, &busyDuringCallback]()
        {
            busyDuringCallback = runner.Busy();
        });

    EXPECT_TRUE(runner.Busy());
    ExecuteAllActions();
    EXPECT_FALSE(runner.Busy());
    EXPECT_FALSE(busyDuringCallback);
}

TEST_F(RegisterTableRunnerTest, onDone_may_start_another_run)
{
    static constexpr std::array<drivers::RegisterStep, 1> table{ { drivers::RegisterStep::Write(0x01, 0x11) } };

    testing::InSequence seq;
    EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x11 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x11 }));
    EXPECT_CALL(done, callback());

    int callCount{ 0 };
    runner.Run(infra::MakeRange(table), [this, &callCount]()
        {
            ++callCount;
            if (callCount == 1)
                runner.Run(infra::MakeRange(table), [this]()
                    {
                        done.callback();
                    });
        });

    ExecuteAllActions();
}
