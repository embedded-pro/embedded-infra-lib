#include "infra/stream/StringOutputStream.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/fsm/TransitionTableAnalysis.hpp"
#include "services/fsm/test/JobLifecycle.hpp"
#include "services/fsm/test_doubles/StateMachineTester.hpp"
#include "services/tracer/Tracer.hpp"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <optional>

namespace
{
    using example::JobLifecycle;
    using Machine = JobLifecycle::Machine;
    using StateId = example::StateId;
    using Event = example::Event;
    using Tester = services::StateMachineTester<example::State, Event>;
    using Analysis = services::TransitionTableAnalysis<Machine>;

    static_assert(Analysis(JobLifecycle::Rows()).IsValid(StateId::Of<example::Idle>(), JobLifecycle::Rules(), services::Severity::warning));

    class TracerToStreamWithoutHeader
        : public services::TracerToStream
    {
    public:
        using services::TracerToStream::TracerToStream;

    protected:
        void InsertHeader() override
        {}
    };

    constexpr example::Settings activatableSettings{ 3, true };
    constexpr example::Settings settingsThatCannotActivate{ 3, false };
}

class StateMachineIntegrationTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    void PrepareAndCommit(example::Settings settings)
    {
        job->StateMachine().Dispatch(example::Prepare{});
        preparation.Complete(settings);
        ExecuteAllActions();
        storage.Complete();
        ExecuteAllActions();
    }

    void RunToActive()
    {
        PrepareAndCommit(activatableSettings);
        job->StateMachine().Dispatch(example::Activate{});
    }

    example::AsyncOperationStub preparation;
    example::StorageStub storage;
    example::WorkerStub worker;
    infra::StringOutputStream::WithStorage<1024> stream;
    TracerToStreamWithoutHeader tracer{ stream };
    std::optional<JobLifecycle> job{ std::in_place, preparation, storage, worker, tracer };
};

TEST_F(StateMachineIntegrationTest, happy_path_to_active_traces_every_transition)
{
    job->Start();
    RunToActive();

    EXPECT_TRUE(job->StateMachine().Is<example::Active>());
    EXPECT_EQ(1, preparation.started);
    EXPECT_EQ(3, storage.saved->value);
    EXPECT_EQ(1, worker.starts);
    EXPECT_EQ(
        "\r\nfsm: started in Idle"
        "\r\nfsm: Idle --Prepare--> Preparing"
        "\r\nfsm: Preparing --Prepared--> Committing"
        "\r\nfsm: Committing --Committed--> Ready"
        "\r\nfsm: Ready --Activate--> Active",
        stream.Storage());
}

TEST_F(StateMachineIntegrationTest, completions_arrive_through_event_dispatcher_not_inline)
{
    job->Start();
    job->StateMachine().Dispatch(example::Prepare{});

    preparation.Complete(activatableSettings);
    EXPECT_TRUE(job->StateMachine().Is<example::Preparing>());

    ExecuteAllActions();
    EXPECT_TRUE(job->StateMachine().Is<example::Committing>());
}

TEST_F(StateMachineIntegrationTest, failed_preparation_returns_to_idle_via_fallback_row)
{
    job->Start();
    job->StateMachine().Dispatch(example::Prepare{});

    preparation.Complete(std::nullopt);
    ExecuteAllActions();

    EXPECT_TRUE(job->StateMachine().Is<example::Idle>());
    EXPECT_FALSE(storage.saved.has_value());
}

TEST_F(StateMachineIntegrationTest, preparation_completion_after_fault_is_discarded)
{
    job->Start();
    job->StateMachine().Dispatch(example::Prepare{});
    job->StateMachine().Dispatch(example::FaultDetected{ example::FaultCode::overload });
    EXPECT_EQ(1, preparation.cancelled);
    stream.Storage().clear();

    preparation.Complete(activatableSettings);
    ExecuteAllActions();

    EXPECT_TRUE(job->StateMachine().Is<example::Fault>());
    EXPECT_EQ(example::FaultCode::overload, std::get<example::Fault>(job->StateMachine().CurrentState()).code);
    EXPECT_EQ("\r\nfsm: discarded Prepared in Fault", stream.Storage());
}

TEST_F(StateMachineIntegrationTest, preparation_timeout_cancels_operation_and_returns_to_idle)
{
    job->Start();
    job->StateMachine().Dispatch(example::Prepare{});

    ForwardTime(std::chrono::seconds(9));
    EXPECT_TRUE(job->StateMachine().Is<example::Preparing>());

    ForwardTime(std::chrono::seconds(1));
    EXPECT_TRUE(job->StateMachine().Is<example::Idle>());
    EXPECT_EQ(1, preparation.cancelled);
}

TEST_F(StateMachineIntegrationTest, activate_is_rejected_when_guard_refuses)
{
    job->Start();
    PrepareAndCommit(settingsThatCannotActivate);
    stream.Storage().clear();

    EXPECT_EQ(services::DispatchResult::rejected, job->StateMachine().Dispatch(example::Activate{}));

    EXPECT_TRUE(job->StateMachine().Is<example::Ready>());
    EXPECT_EQ(0, worker.starts);
    EXPECT_EQ("\r\nfsm: rejected Activate in Ready", stream.Storage());
}

TEST_F(StateMachineIntegrationTest, input_is_internal_and_applied_without_leaving_active)
{
    job->Start();
    RunToActive();
    stream.Storage().clear();

    job->StateMachine().Dispatch(example::Input{ 2.5f });

    EXPECT_NEAR(2.5f, worker.input, 1e-6f);
    EXPECT_EQ(1, worker.starts);
    EXPECT_EQ(0, worker.stops);
    EXPECT_EQ("\r\nfsm: handled Input in Active", stream.Storage());
}

TEST_F(StateMachineIntegrationTest, fault_from_any_state_runs_active_exit_only_when_active)
{
    job->Start();
    RunToActive();

    job->StateMachine().Dispatch(example::FaultDetected{ example::FaultCode::lostConnection });
    EXPECT_EQ(1, worker.stops);
    EXPECT_EQ(example::FaultCode::lostConnection, std::get<example::Fault>(job->StateMachine().CurrentState()).code);

    job->StateMachine().Dispatch(example::Reset{});
    job->StateMachine().Dispatch(example::FaultDetected{ example::FaultCode::overload });
    EXPECT_EQ(1, worker.stops);
    EXPECT_TRUE(job->StateMachine().Is<example::Fault>());
}

TEST_F(StateMachineIntegrationTest, abort_result_depends_on_source_state)
{
    job->Start();
    RunToActive();

    EXPECT_EQ(example::AbortResult::stoppedWorker, job->AbortRequested());
    EXPECT_TRUE(job->StateMachine().Is<example::Idle>());
    EXPECT_EQ(1, worker.stops);

    EXPECT_EQ(example::AbortResult::alreadyIdle, job->AbortRequested());
    EXPECT_TRUE(job->StateMachine().Is<example::Idle>());
}

TEST_F(StateMachineIntegrationTest, forbidden_matrix_matches_table)
{
    constexpr std::array<Event, 10> sampleEvents{ example::Prepare{}, example::Prepared{}, example::Committed{}, example::Activate{}, example::Deactivate{}, example::Input{ 1.0f }, example::FaultDetected{ example::FaultCode::overload }, example::Abort{}, example::Reset{}, example::Timeout{} };
    constexpr std::array<Event, 0> toIdle{};
    constexpr std::array<Event, 1> toPreparing{ example::Prepare{} };
    constexpr std::array<Event, 2> toCommitting{ example::Prepare{}, example::Prepared{} };
    constexpr std::array<Event, 3> toReady{ example::Prepare{}, example::Prepared{}, example::Committed{} };
    constexpr std::array<Event, 4> toActive{ example::Prepare{}, example::Prepared{}, example::Committed{}, example::Activate{} };
    constexpr std::array<Event, 1> toFault{ example::FaultDetected{} };
    const std::array<infra::MemoryRange<const Event>, StateId::count> paths{ infra::MakeRange(toIdle), infra::MakeRange(toPreparing), infra::MakeRange(toCommitting), infra::MakeRange(toReady), infra::MakeRange(toActive), infra::MakeRange(toFault) };

    Tester::ExpectForbiddenMatrixMatchesTable([&](StateId state) -> Machine&
        {
            job.emplace(preparation, storage, worker, tracer);
            preparation.SetResult(activatableSettings);
            stream.Storage().clear();
            job->Start();
            Tester::DriveTo(job->StateMachine(), state, paths[state.Index()]);
            return job->StateMachine();
        },
        infra::MakeRange(sampleEvents));
}
