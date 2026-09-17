#include "drivers/imu/mpu9250/Mpu9250StepRunner.hpp"
#include "drivers/imu/mpu9250/test/Mpu9250BusAccessMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <vector>

namespace
{
    using Runner = drivers::Mpu9250StepRunner;

    class Mpu9250StepRunnerTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        testing::StrictMock<drivers::Mpu9250BusAccessMock> bus;
        infra::AccessedBySharedPtr sharedAccess{ infra::emptyFunction };
        Runner runner{ bus, sharedAccess };
        uint8_t destination = 0;
    };
}

TEST_F(Mpu9250StepRunnerTest, a_write_register_step_writes_the_value)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x6b, std::vector<uint8_t>{ 0x80 }));

    runner.Clear();
    runner.Push(Runner::WriteRegister{ 0x6b, 0x80 });

    infra::VerifyingFunction<void()> done;
    runner.Start(done);

    ExecuteAllActions();
}

TEST_F(Mpu9250StepRunnerTest, a_read_burst_step_reads_into_the_given_range)
{
    EXPECT_CALL(bus, ReadRegisterMock(0x75, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x71 }));

    runner.Clear();
    runner.Push(Runner::ReadBurst{ 0x75, infra::MakeByteRange(destination) });

    infra::VerifyingFunction<void()> done;
    runner.Start(done);

    ExecuteAllActions();

    EXPECT_EQ(0x71, destination);
}

TEST_F(Mpu9250StepRunnerTest, a_modify_register_step_reads_then_writes_the_masked_value)
{
    EXPECT_CALL(bus, ReadRegisterMock(0x6a, 1)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x41 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x6a, std::vector<uint8_t>{ 0x05 }));

    runner.Clear();
    runner.Push(Runner::ModifyRegister{ 0x6a, 0x40, 0x04 });

    infra::VerifyingFunction<void()> done;
    runner.Start(done);

    ExecuteAllActions();
}

TEST_F(Mpu9250StepRunnerTest, a_delay_step_waits_before_continuing)
{
    runner.Clear();
    runner.Push(Runner::Delay{ std::chrono::milliseconds(100) });

    bool finished = false;
    runner.Start([&finished]()
        {
            finished = true;
        });

    ForwardTime(std::chrono::milliseconds(99));
    EXPECT_FALSE(finished);

    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_TRUE(finished);
}

TEST_F(Mpu9250StepRunnerTest, steps_run_in_the_order_they_were_pushed)
{
    testing::InSequence sequence;

    EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x11 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x22 }));
    EXPECT_CALL(bus, WriteRegisterMock(0x03, std::vector<uint8_t>{ 0x33 }));

    runner.Clear();
    runner.Push(Runner::WriteRegister{ 0x01, 0x11 });
    runner.Push(Runner::WriteRegister{ 0x02, 0x22 });
    runner.Push(Runner::WriteRegister{ 0x03, 0x33 });

    infra::VerifyingFunction<void()> done;
    runner.Start(done);

    ExecuteAllActions();
}

TEST_F(Mpu9250StepRunnerTest, an_invoke_step_runs_inline_and_the_list_advances)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x22 }));

    bool invoked = false;

    runner.Clear();
    runner.Push(Runner::Invoke{ [&invoked]()
        {
            invoked = true;
        } });
    runner.Push(Runner::WriteRegister{ 0x02, 0x22 });

    infra::VerifyingFunction<void()> done;
    runner.Start(done);

    EXPECT_TRUE(invoked);

    ExecuteAllActions();
}

TEST_F(Mpu9250StepRunnerTest, an_await_step_suspends_the_list_until_continue_is_called)
{
    runner.Clear();
    runner.Push(Runner::Await{ [this]()
        {
            EXPECT_CALL(bus, WriteRegisterMock(0x02, std::vector<uint8_t>{ 0x22 }));
        } });
    runner.Push(Runner::WriteRegister{ 0x02, 0x22 });

    bool finished = false;
    runner.Start([&finished]()
        {
            finished = true;
        });

    ExecuteAllActions();
    EXPECT_FALSE(finished);
    EXPECT_TRUE(runner.Busy());

    runner.Continue();
    ExecuteAllActions();

    EXPECT_TRUE(finished);
}

TEST_F(Mpu9250StepRunnerTest, abort_runs_no_further_steps_and_no_completion)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x11 }));

    runner.Clear();
    runner.Push(Runner::WriteRegister{ 0x01, 0x11 });
    runner.Push(Runner::Invoke{ [this]()
        {
            runner.Abort();
        } });
    runner.Push(Runner::WriteRegister{ 0x02, 0x22 });

    bool finished = false;
    runner.Start([&finished]()
        {
            finished = true;
        });

    ExecuteAllActions();

    EXPECT_FALSE(finished);
    EXPECT_FALSE(runner.Busy());
}

TEST_F(Mpu9250StepRunnerTest, busy_tracks_the_run)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x11 }));

    EXPECT_FALSE(runner.Busy());

    runner.Clear();
    runner.Push(Runner::WriteRegister{ 0x01, 0x11 });

    infra::VerifyingFunction<void()> done;
    runner.Start(done);

    EXPECT_TRUE(runner.Busy());

    ExecuteAllActions();

    EXPECT_FALSE(runner.Busy());
}

TEST_F(Mpu9250StepRunnerTest, an_empty_list_completes_immediately)
{
    runner.Clear();

    infra::VerifyingFunction<void()> done;
    runner.Start(done);

    ExecuteAllActions();

    EXPECT_FALSE(runner.Busy());
}

TEST_F(Mpu9250StepRunnerTest, the_runner_stays_busy_until_its_completion_has_been_delivered)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x11 }));

    bus.completeAutomatically = false;

    runner.Clear();
    runner.Push(Runner::WriteRegister{ 0x01, 0x11 });

    int completions = 0;
    runner.Start([&completions]()
        {
            completions += 1;
        });

    bus.CompletePending();

    // The last step is done but the completion is only queued, so a Start() here would otherwise
    // overwrite onDone and make this run report the next run's callback
    EXPECT_TRUE(runner.Busy());
    EXPECT_EQ(0, completions);

    ExecuteAllActions();

    EXPECT_FALSE(runner.Busy());
    EXPECT_EQ(1, completions);
}

TEST_F(Mpu9250StepRunnerTest, an_aborted_run_delivers_no_completion_even_once_queued)
{
    EXPECT_CALL(bus, WriteRegisterMock(0x01, std::vector<uint8_t>{ 0x11 }));

    runner.Clear();
    runner.Push(Runner::WriteRegister{ 0x01, 0x11 });
    runner.Push(Runner::Invoke{ [this]()
        {
            runner.Abort();
        } });

    int completions = 0;
    runner.Start([&completions]()
        {
            completions += 1;
        });

    ExecuteAllActions();

    EXPECT_EQ(0, completions);
}
