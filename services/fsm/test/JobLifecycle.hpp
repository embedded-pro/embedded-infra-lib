#ifndef SERVICES_FSM_TEST_JOB_LIFECYCLE_HPP
#define SERVICES_FSM_TEST_JOB_LIFECYCLE_HPP

#include "infra/event/EventDispatcher.hpp"
#include "infra/util/Function.hpp"
#include "services/fsm/StateMachineTracer.hpp"
#include "services/fsm/StateTimeouts.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "services/tracer/Tracer.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

namespace example
{
    struct Settings
    {
        int value{ 0 };
        bool activatable{ false };
    };

    enum class FaultCode : uint8_t
    {
        none,
        overload,
        lostConnection
    };

    enum class AbortResult : uint8_t
    {
        stoppedWorker,
        alreadyIdle
    };

    class AsyncOperationStub
    {
    public:
        void Start(const infra::Function<void()>& onDone)
        {
            ++started;
            this->onDone = onDone;
        }

        void Cancel()
        {
            ++cancelled;
        }

        void SetResult(std::optional<Settings> newResult)
        {
            result = newResult;
        }

        std::optional<Settings> Result() const
        {
            return result;
        }

        void Complete(std::optional<Settings> newResult)
        {
            result = newResult;
            infra::EventDispatcher::Instance().Schedule(onDone);
        }

        int started{ 0 };
        int cancelled{ 0 };

    private:
        infra::Function<void()> onDone;
        std::optional<Settings> result;
    };

    class StorageStub
    {
    public:
        void Save(const Settings& settings, const infra::Function<void()>& onDone)
        {
            saved = settings;
            this->onDone = onDone;
        }

        void Complete()
        {
            infra::EventDispatcher::Instance().Schedule(onDone);
        }

        std::optional<Settings> saved;

    private:
        infra::Function<void()> onDone;
    };

    class WorkerStub
    {
    public:
        void Start()
        {
            ++starts;
        }

        void Stop()
        {
            ++stops;
        }

        void Apply(float value)
        {
            input = value;
        }

        int starts{ 0 };
        int stops{ 0 };
        float input{ 0.0f };
    };

    struct Idle
    {
        static constexpr const char* name{ "Idle" };
    };

    struct Preparing
    {
        static constexpr const char* name{ "Preparing" };
    };

    struct Committing
    {
        static constexpr const char* name{ "Committing" };
        Settings settings;
    };

    struct Ready
    {
        static constexpr const char* name{ "Ready" };
        Settings settings;
    };

    struct Active
    {
        static constexpr const char* name{ "Active" };

        void OnEntry()
        {
            worker.Start();
        }

        void OnExit()
        {
            worker.Stop();
        }

        WorkerStub& worker;
        Settings settings;
    };

    struct Fault
    {
        static constexpr const char* name{ "Fault" };
        FaultCode code{ FaultCode::none };
    };

    using State = std::variant<Idle, Preparing, Committing, Ready, Active, Fault>;

    struct Prepare
    {
        static constexpr const char* name{ "Prepare" };
    };

    struct Prepared
    {
        static constexpr const char* name{ "Prepared" };
    };

    struct Committed
    {
        static constexpr const char* name{ "Committed" };
    };

    struct Activate
    {
        static constexpr const char* name{ "Activate" };
    };

    struct Deactivate
    {
        static constexpr const char* name{ "Deactivate" };
    };

    struct Input
    {
        static constexpr const char* name{ "Input" };
        float value{ 0.0f };
    };

    struct FaultDetected
    {
        static constexpr const char* name{ "FaultDetected" };
        FaultCode code{ FaultCode::none };
    };

    struct Abort
    {
        static constexpr const char* name{ "Abort" };
    };

    struct Reset
    {
        static constexpr const char* name{ "Reset" };
    };

    struct Timeout
    {
        static constexpr const char* name{ "Timeout" };
    };

    using Event = std::variant<Prepare, Prepared, Committed, Activate, Deactivate, Input, FaultDetected, Abort, Reset, Timeout>;

    using StateId = services::AlternativeId<State>;

    inline constexpr std::array<services::StateTimeout<State, Event>, 1> timeouts{ {
        { StateId::Of<Preparing>(), std::chrono::seconds(10), Event{ Timeout{} } },
    } };

    class JobLifecycle
    {
    public:
        using Machine = services::TableStateMachine<State, Event, JobLifecycle>;

        JobLifecycle(AsyncOperationStub& preparation, StorageStub& storage, WorkerStub& worker, services::Tracer& tracer)
            : preparation(preparation)
            , storage(storage)
            , worker(worker)
            , fsm(*this, Table())
            , stateMachineTracer(fsm, tracer)
        {}

        void Start()
        {
            fsm.Start<Idle>();
        }

        AbortResult AbortRequested()
        {
            auto from = fsm.CurrentStateId();
            fsm.Dispatch(Abort{});
            return from.Is<Active>() ? AbortResult::stoppedWorker : AbortResult::alreadyIdle;
        }

        Machine& StateMachine()
        {
            return fsm;
        }

        static constexpr std::array<Machine::Transition, 12> Rows();

    private:
        static Machine::Table Table();
        static constexpr std::array<Machine::Transition, 5> PreparationRows();
        static constexpr std::array<Machine::Transition, 3> OperationRows();
        static constexpr std::array<Machine::Transition, 4> SafetyRows();

    private:
        AsyncOperationStub& preparation;
        StorageStub& storage;
        WorkerStub& worker;
        Machine::WithStorage<4> fsm;
        services::StateMachineTracer<State, Event> stateMachineTracer;
        services::StateTimeouts<State, Event> stateTimeouts{ fsm, infra::MakeRange(timeouts) };
    };

    constexpr std::array<JobLifecycle::Machine::Transition, 5> JobLifecycle::PreparationRows()
    {
        return {
            Machine::Row<Idle, Prepare, Preparing>(nullptr, [](JobLifecycle& job, Idle&, const Prepare&)
                {
                    job.preparation.Start(job.fsm.Completion<Prepared>());
                    return Preparing{};
                }),
            Machine::Row<Preparing, Prepared, Committing>([](JobLifecycle& job, const Preparing&, const Prepared&)
                {
                    return job.preparation.Result().has_value();
                },
                [](JobLifecycle& job, Preparing&, const Prepared&)
                {
                    job.storage.Save(*job.preparation.Result(), job.fsm.Completion<Committed>());
                    return Committing{ *job.preparation.Result() };
                }),
            Machine::Row<Preparing, Prepared, Idle>(),
            Machine::Row<Preparing, Timeout, Idle>(nullptr, [](JobLifecycle& job, Preparing&, const Timeout&)
                {
                    job.preparation.Cancel();
                    return Idle{};
                }),
            Machine::Row<Committing, Committed, Ready>(nullptr, [](JobLifecycle&, Committing& from, const Committed&)
                {
                    return Ready{ from.settings };
                }),
        };
    }

    constexpr std::array<JobLifecycle::Machine::Transition, 3> JobLifecycle::OperationRows()
    {
        return {
            Machine::Row<Ready, Activate, Active>([](JobLifecycle&, const Ready& from, const Activate&)
                {
                    return from.settings.activatable;
                },
                [](JobLifecycle& job, Ready& from, const Activate&)
                {
                    return Active{ job.worker, from.settings };
                }),
            Machine::Row<Active, Deactivate, Ready>(nullptr, [](JobLifecycle&, Active& from, const Deactivate&)
                {
                    return Ready{ from.settings };
                }),
            Machine::InternalRow<Active, Input>([](JobLifecycle& job, Active&, const Input& event)
                {
                    job.worker.Apply(event.value);
                }),
        };
    }

    constexpr std::array<JobLifecycle::Machine::Transition, 4> JobLifecycle::SafetyRows()
    {
        return {
            Machine::RowFromAny<FaultDetected, Fault>(nullptr, [](JobLifecycle& job, State&, const FaultDetected& event)
                {
                    job.preparation.Cancel();
                    return Fault{ event.code };
                }),
            Machine::Row<Fault, Reset, Idle>(),
            Machine::InternalRow<Idle, Abort>(),
            Machine::RowFromAny<Abort, Idle>(nullptr, [](JobLifecycle& job, State&, const Abort&)
                {
                    job.preparation.Cancel();
                    return Idle{};
                }),
        };
    }

    constexpr std::array<JobLifecycle::Machine::Transition, 12> JobLifecycle::Rows()
    {
        return services::JoinRows(PreparationRows(), OperationRows(), SafetyRows());
    }

    inline JobLifecycle::Machine::Table JobLifecycle::Table()
    {
        static constexpr auto rows = Rows();
        return infra::MakeRange(rows);
    }
}

#endif
