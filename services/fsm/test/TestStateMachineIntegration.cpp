#include "infra/event/EventDispatcher.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/fsm/StateMachineTracer.hpp"
#include "services/fsm/StateTimeouts.hpp"
#include "services/fsm/TableStateMachine.hpp"
#include "services/fsm/test_doubles/StateMachineTester.hpp"
#include "services/tracer/Tracer.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <optional>

namespace
{
    struct CalibrationData
    {
        int offset{ 0 };
        bool rotorReferenceValid{ false };
    };

    enum class FaultCode : uint8_t
    {
        none,
        overCurrent,
        overVoltage
    };

    enum class StopResult : uint8_t
    {
        stoppedDrive,
        alreadyStopped
    };

    class CalibrationServiceStub
    {
    public:
        void Start(const infra::Function<void()>& onDone)
        {
            ++started;
            this->onDone = onDone;
        }

        void Abort()
        {
            ++aborted;
        }

        void SetResult(std::optional<CalibrationData> newResult)
        {
            result = newResult;
        }

        std::optional<CalibrationData> Result() const
        {
            return result;
        }

        void Complete(std::optional<CalibrationData> newResult)
        {
            result = newResult;
            infra::EventDispatcher::Instance().Schedule(onDone);
        }

        int started{ 0 };
        int aborted{ 0 };

    private:
        infra::Function<void()> onDone;
        std::optional<CalibrationData> result;
    };

    class StorageStub
    {
    public:
        void Save(const CalibrationData& data, const infra::Function<void()>& onDone)
        {
            saved = data;
            this->onDone = onDone;
        }

        void Complete()
        {
            infra::EventDispatcher::Instance().Schedule(onDone);
        }

        std::optional<CalibrationData> saved;

    private:
        infra::Function<void()> onDone;
    };

    class DriveStub
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
            setpoint = value;
        }

        int starts{ 0 };
        int stops{ 0 };
        float setpoint{ 0.0f };
    };

    struct Idle
    {
        static constexpr const char* name{ "Idle" };
    };

    struct Calibrating
    {
        static constexpr const char* name{ "Calibrating" };
    };

    struct Saving
    {
        static constexpr const char* name{ "Saving" };
        CalibrationData data;
    };

    struct Ready
    {
        static constexpr const char* name{ "Ready" };
        CalibrationData data;
    };

    struct Enabled
    {
        static constexpr const char* name{ "Enabled" };

        void OnEntry()
        {
            drive.Start();
        }

        void OnExit()
        {
            drive.Stop();
        }

        DriveStub& drive;
        CalibrationData data;
    };

    struct Fault
    {
        static constexpr const char* name{ "Fault" };
        FaultCode code{ FaultCode::none };
    };

    using State = std::variant<Idle, Calibrating, Saving, Ready, Enabled, Fault>;

    struct Calibrate
    {
        static constexpr const char* name{ "Calibrate" };
    };

    struct CalibrationDone
    {
        static constexpr const char* name{ "CalibrationDone" };
    };

    struct Saved
    {
        static constexpr const char* name{ "Saved" };
    };

    struct Enable
    {
        static constexpr const char* name{ "Enable" };
    };

    struct Disable
    {
        static constexpr const char* name{ "Disable" };
    };

    struct Setpoint
    {
        static constexpr const char* name{ "Setpoint" };
        float value{ 0.0f };
    };

    struct FaultDetected
    {
        static constexpr const char* name{ "FaultDetected" };
        FaultCode code{ FaultCode::none };
    };

    struct EmergencyStop
    {
        static constexpr const char* name{ "EmergencyStop" };
    };

    struct ClearFault
    {
        static constexpr const char* name{ "ClearFault" };
    };

    struct Timeout
    {
        static constexpr const char* name{ "Timeout" };
    };

    using Event = std::variant<Calibrate, CalibrationDone, Saved, Enable, Disable, Setpoint, FaultDetected, EmergencyStop, ClearFault, Timeout>;

    class DeviceLifecycle;

    using StateId = services::AlternativeId<State>;
    using EventId = services::AlternativeId<Event>;
    using Machine = services::TableStateMachine<State, Event, DeviceLifecycle>;
    using Tester = services::StateMachineTester<State, Event>;

    constexpr std::array<services::StateTimeout<State, Event>, 1> timeouts{ {
        { StateId::Of<Calibrating>(), std::chrono::seconds(10), Event{ Timeout{} } },
    } };

    class DeviceLifecycle
    {
    public:
        DeviceLifecycle(CalibrationServiceStub& calibration, StorageStub& storage, DriveStub& drive, services::Tracer& tracer)
            : calibration(calibration)
            , storage(storage)
            , drive(drive)
            , fsm(*this, Table())
            , stateMachineTracer(fsm, tracer)
        {}

        void Start()
        {
            fsm.Start<Idle>();
        }

        StopResult EmergencyStopRequested()
        {
            auto from = fsm.CurrentStateId();
            fsm.Dispatch(EmergencyStop{});
            return from.Is<Enabled>() ? StopResult::stoppedDrive : StopResult::alreadyStopped;
        }

        Machine& StateMachine()
        {
            return fsm;
        }

    private:
        static Machine::Table Table();
        static constexpr std::array<Machine::Transition, 5> CalibrationRows();
        static constexpr std::array<Machine::Transition, 3> OperationRows();
        static constexpr std::array<Machine::Transition, 4> SafetyRows();

    private:
        CalibrationServiceStub& calibration;
        StorageStub& storage;
        DriveStub& drive;
        Machine::WithStorage<4> fsm;
        services::StateMachineTracer<State, Event> stateMachineTracer;
        services::StateTimeouts<State, Event> stateTimeouts{ fsm, infra::MakeRange(timeouts) };
    };

    constexpr std::array<Machine::Transition, 5> DeviceLifecycle::CalibrationRows()
    {
        return {
            Machine::Row<Idle, Calibrate, Calibrating>(nullptr, [](DeviceLifecycle& device, Idle&, const Calibrate&)
                {
                    device.calibration.Start(device.fsm.Completion<CalibrationDone>());
                    return Calibrating{};
                }),
            Machine::Row<Calibrating, CalibrationDone, Saving>([](DeviceLifecycle& device, const Calibrating&, const CalibrationDone&)
                {
                    return device.calibration.Result().has_value();
                },
                [](DeviceLifecycle& device, Calibrating&, const CalibrationDone&)
                {
                    device.storage.Save(*device.calibration.Result(), device.fsm.Completion<Saved>());
                    return Saving{ *device.calibration.Result() };
                }),
            Machine::Row<Calibrating, CalibrationDone, Idle>(),
            Machine::Row<Calibrating, Timeout, Idle>(nullptr, [](DeviceLifecycle& device, Calibrating&, const Timeout&)
                {
                    device.calibration.Abort();
                    return Idle{};
                }),
            Machine::Row<Saving, Saved, Ready>(nullptr, [](DeviceLifecycle&, Saving& from, const Saved&)
                {
                    return Ready{ from.data };
                }),
        };
    }

    constexpr std::array<Machine::Transition, 3> DeviceLifecycle::OperationRows()
    {
        return {
            Machine::Row<Ready, Enable, Enabled>([](DeviceLifecycle&, const Ready& from, const Enable&)
                {
                    return from.data.rotorReferenceValid;
                },
                [](DeviceLifecycle& device, Ready& from, const Enable&)
                {
                    return Enabled{ device.drive, from.data };
                }),
            Machine::Row<Enabled, Disable, Ready>(nullptr, [](DeviceLifecycle&, Enabled& from, const Disable&)
                {
                    return Ready{ from.data };
                }),
            Machine::InternalRow<Enabled, Setpoint>([](DeviceLifecycle& device, Enabled&, const Setpoint& event)
                {
                    device.drive.Apply(event.value);
                }),
        };
    }

    constexpr std::array<Machine::Transition, 4> DeviceLifecycle::SafetyRows()
    {
        return {
            Machine::RowFromAny<FaultDetected, Fault>(nullptr, [](DeviceLifecycle& device, State&, const FaultDetected& event)
                {
                    device.calibration.Abort();
                    return Fault{ event.code };
                }),
            Machine::Row<Fault, ClearFault, Idle>(),
            Machine::InternalRow<Idle, EmergencyStop>(),
            Machine::RowFromAny<EmergencyStop, Idle>(nullptr, [](DeviceLifecycle& device, State&, const EmergencyStop&)
                {
                    device.calibration.Abort();
                    return Idle{};
                }),
        };
    }

    Machine::Table DeviceLifecycle::Table()
    {
        static constexpr auto rows = services::JoinRows(CalibrationRows(), OperationRows(), SafetyRows());
        return infra::MakeRange(rows);
    }

    class TracerToStreamWithoutHeader
        : public services::TracerToStream
    {
    public:
        using services::TracerToStream::TracerToStream;

    protected:
        void InsertHeader() override
        {}
    };

    constexpr CalibrationData validCalibration{ 3, true };
    constexpr CalibrationData calibrationWithoutRotorReference{ 3, false };
}

class StateMachineIntegrationTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    void CalibrateAndSave(CalibrationData data)
    {
        device->StateMachine().Dispatch(Calibrate{});
        calibration.Complete(data);
        ExecuteAllActions();
        storage.Complete();
        ExecuteAllActions();
    }

    void BootToEnabled()
    {
        CalibrateAndSave(validCalibration);
        device->StateMachine().Dispatch(Enable{});
    }

    CalibrationServiceStub calibration;
    StorageStub storage;
    DriveStub drive;
    infra::StringOutputStream::WithStorage<1024> stream;
    TracerToStreamWithoutHeader tracer{ stream };
    std::optional<DeviceLifecycle> device{ std::in_place, calibration, storage, drive, tracer };
};

TEST_F(StateMachineIntegrationTest, boot_to_enabled_happy_path_traces_every_transition)
{
    device->Start();
    BootToEnabled();

    EXPECT_TRUE(device->StateMachine().Is<Enabled>());
    EXPECT_EQ(1, calibration.started);
    EXPECT_EQ(3, storage.saved->offset);
    EXPECT_EQ(1, drive.starts);
    EXPECT_EQ(
        "\r\nfsm: started in Idle"
        "\r\nfsm: Idle --Calibrate--> Calibrating"
        "\r\nfsm: Calibrating --CalibrationDone--> Saving"
        "\r\nfsm: Saving --Saved--> Ready"
        "\r\nfsm: Ready --Enable--> Enabled",
        stream.Storage());
}

TEST_F(StateMachineIntegrationTest, completions_arrive_through_event_dispatcher_not_inline)
{
    device->Start();
    device->StateMachine().Dispatch(Calibrate{});

    calibration.Complete(validCalibration);
    EXPECT_TRUE(device->StateMachine().Is<Calibrating>());

    ExecuteAllActions();
    EXPECT_TRUE(device->StateMachine().Is<Saving>());
}

TEST_F(StateMachineIntegrationTest, failed_calibration_returns_to_idle_via_fallback_row)
{
    device->Start();
    device->StateMachine().Dispatch(Calibrate{});

    calibration.Complete(std::nullopt);
    ExecuteAllActions();

    EXPECT_TRUE(device->StateMachine().Is<::Idle>());
    EXPECT_FALSE(storage.saved.has_value());
}

TEST_F(StateMachineIntegrationTest, calibration_completion_after_fault_is_discarded)
{
    device->Start();
    device->StateMachine().Dispatch(Calibrate{});
    device->StateMachine().Dispatch(FaultDetected{ FaultCode::overCurrent });
    EXPECT_EQ(1, calibration.aborted);
    stream.Storage().clear();

    calibration.Complete(validCalibration);
    ExecuteAllActions();

    EXPECT_TRUE(device->StateMachine().Is<Fault>());
    EXPECT_EQ(FaultCode::overCurrent, std::get<Fault>(device->StateMachine().CurrentState()).code);
    EXPECT_EQ("\r\nfsm: discarded CalibrationDone in Fault", stream.Storage());
}

TEST_F(StateMachineIntegrationTest, calibration_timeout_aborts_service_and_returns_to_idle)
{
    device->Start();
    device->StateMachine().Dispatch(Calibrate{});

    ForwardTime(std::chrono::seconds(9));
    EXPECT_TRUE(device->StateMachine().Is<Calibrating>());

    ForwardTime(std::chrono::seconds(1));
    EXPECT_TRUE(device->StateMachine().Is<::Idle>());
    EXPECT_EQ(1, calibration.aborted);
}

TEST_F(StateMachineIntegrationTest, enable_is_rejected_without_rotor_reference)
{
    device->Start();
    CalibrateAndSave(calibrationWithoutRotorReference);
    stream.Storage().clear();

    EXPECT_EQ(services::DispatchResult::rejected, device->StateMachine().Dispatch(Enable{}));

    EXPECT_TRUE(device->StateMachine().Is<Ready>());
    EXPECT_EQ(0, drive.starts);
    EXPECT_EQ("\r\nfsm: rejected Enable in Ready", stream.Storage());
}

TEST_F(StateMachineIntegrationTest, setpoint_is_internal_and_applied_without_leaving_enabled)
{
    device->Start();
    BootToEnabled();
    stream.Storage().clear();

    device->StateMachine().Dispatch(Setpoint{ 2.5f });

    EXPECT_NEAR(2.5f, drive.setpoint, 1e-6f);
    EXPECT_EQ(1, drive.starts);
    EXPECT_EQ(0, drive.stops);
    EXPECT_EQ("\r\nfsm: handled Setpoint in Enabled", stream.Storage());
}

TEST_F(StateMachineIntegrationTest, fault_from_any_state_runs_enabled_exit_only_when_enabled)
{
    device->Start();
    BootToEnabled();

    device->StateMachine().Dispatch(FaultDetected{ FaultCode::overVoltage });
    EXPECT_EQ(1, drive.stops);
    EXPECT_EQ(FaultCode::overVoltage, std::get<Fault>(device->StateMachine().CurrentState()).code);

    device->StateMachine().Dispatch(ClearFault{});
    device->StateMachine().Dispatch(FaultDetected{ FaultCode::overCurrent });
    EXPECT_EQ(1, drive.stops);
    EXPECT_TRUE(device->StateMachine().Is<Fault>());
}

TEST_F(StateMachineIntegrationTest, emergency_stop_result_depends_on_source_state)
{
    device->Start();
    BootToEnabled();

    EXPECT_EQ(StopResult::stoppedDrive, device->EmergencyStopRequested());
    EXPECT_TRUE(device->StateMachine().Is<::Idle>());
    EXPECT_EQ(1, drive.stops);

    EXPECT_EQ(StopResult::alreadyStopped, device->EmergencyStopRequested());
    EXPECT_TRUE(device->StateMachine().Is<::Idle>());
}

TEST_F(StateMachineIntegrationTest, forbidden_matrix_matches_table)
{
    constexpr std::array<Event, 10> sampleEvents{ Calibrate{}, CalibrationDone{}, Saved{}, Enable{}, Disable{}, Setpoint{ 1.0f }, FaultDetected{ FaultCode::overCurrent }, EmergencyStop{}, ClearFault{}, Timeout{} };
    constexpr std::array<Event, 0> toIdle{};
    constexpr std::array<Event, 1> toCalibrating{ Calibrate{} };
    constexpr std::array<Event, 2> toSaving{ Calibrate{}, CalibrationDone{} };
    constexpr std::array<Event, 3> toReady{ Calibrate{}, CalibrationDone{}, Saved{} };
    constexpr std::array<Event, 4> toEnabled{ Calibrate{}, CalibrationDone{}, Saved{}, Enable{} };
    constexpr std::array<Event, 1> toFault{ FaultDetected{} };
    const std::array<infra::MemoryRange<const Event>, StateId::count> paths{ infra::MakeRange(toIdle), infra::MakeRange(toCalibrating), infra::MakeRange(toSaving), infra::MakeRange(toReady), infra::MakeRange(toEnabled), infra::MakeRange(toFault) };

    Tester::ExpectForbiddenMatrixMatchesTable([&](StateId state) -> Machine&
        {
            device.emplace(calibration, storage, drive, tracer);
            calibration.SetResult(validCalibration);
            stream.Storage().clear();
            device->Start();
            Tester::DriveTo(device->StateMachine(), state, paths[state.Index()]);
            return device->StateMachine();
        },
        infra::MakeRange(sampleEvents));
}
