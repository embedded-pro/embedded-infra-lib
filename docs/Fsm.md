# Finite State Machine

## Introduction

Many components in an embedded application are state machines: a connection is closed, connecting or connected; a motor is idle, calibrating, ready or enabled; a firmware upgrade is downloading, verifying or installing.
Writing such a component by hand tends to scatter the rules over many methods: every command checks the current state, every asynchronous callback checks it again, and the set of forbidden transitions exists only in the head of the author.
The `services/fsm` package provides a generic, heap-less state machine in which the transition table is the single source of truth.
The table is a compile-time constant that lives in flash, it is checked for consistency before the machine starts, every event that is not allowed in the current state is reported rather than silently ignored, and asynchronous completions cannot corrupt the machine when they arrive late.

This package complements two idioms that already exist in the library.
`infra::Sequencer` expresses a linear or structured sequence of asynchronous steps; it is the right tool when there is one path through the work.
Polymorphic states stored in an `infra::PolymorphicVariant` are convenient for protocol parsers where each state handles a fixed set of callbacks.
`services::TableStateMachine` is the right tool when the state graph has many edges, when it must be verified, traced or documented, and when external events may arrive in states where they are not allowed.

## States and events

States and events are classes, collected in a `std::variant`.
A state class owns the data that only exists while that state is active, and may define `OnEntry()` and `OnExit()` members which the machine calls when the state is entered and left.
An event class carries the payload of the event.
Every state and event class provides a `static constexpr const char* name`, which the tracer and the diagram writer use.

```cpp
struct Idle
{
    static constexpr const char* name{ "Idle" };
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

    Drive& drive;
};

using State = std::variant<Idle, Calibrating, Ready, Enabled, Fault>;

struct FaultDetected
{
    static constexpr const char* name{ "FaultDetected" };
    FaultCode code;
};

using Event = std::variant<Calibrate, CalibrationDone, Enable, Disable, FaultDetected>;
```

`services::AlternativeId<Variant>` identifies one alternative of such a variant without holding a value. It is what observers receive, what timeouts are keyed on, and what `CurrentStateId()` returns. `AlternativeId<State>::Of<Enabled>()` names a state class, `id.Is<Enabled>()` tests it and `id.Name()` returns its name.

## The transition table

`services::TableStateMachine<State, Event, Context>` runs a table of rows over a context object, normally the component that owns the machine.
Rows are built with the `constexpr` functions `Row`, `RowFromAny` and `InternalRow`, so that a table declared `static constexpr` is a constant in flash and costs no RAM; the machine itself only holds the current state and a queue for events dispatched while it is busy.
Guards and actions are captureless callables, typically lambdas, that receive the context as their first argument.
A guard receives the context, the current state object and the event and returns whether the row applies; an action receives the context, the current state object and the event and returns the new state object.
Because the callables are captureless they can call whatever the context exposes, including its private members when the table is built inside one of its member functions.

```cpp
class Motor
{
public:
    Motor(Calibration& calibration, Drive& drive)
        : calibration(calibration)
        , drive(drive)
        , fsm(*this, Table())
    {
        fsm.Start<Idle>();
    }

private:
    using Machine = services::TableStateMachine<State, Event, Motor>;

    static Machine::Table Table();

    Calibration& calibration;
    Drive& drive;
    Machine::WithStorage<4> fsm;
};

Motor::Machine::Table Motor::Table()
{
    static constexpr std::array rows{
        Machine::Row<Idle, Calibrate, Calibrating>(nullptr, [](Motor& motor, Idle&, const Calibrate&)
            {
                motor.calibration.Start(motor.fsm.Completion<CalibrationDone>());
                return Calibrating{};
            }),
        Machine::Row<Calibrating, CalibrationDone, Ready>([](Motor& motor, const Calibrating&, const CalibrationDone&)
            {
                return motor.calibration.Result().has_value();
            },
            [](Motor& motor, Calibrating&, const CalibrationDone&)
            {
                return Ready{ *motor.calibration.Result() };
            }),
        Machine::Row<Calibrating, CalibrationDone, Idle>(),
        Machine::Row<Ready, Enable, Enabled>(nullptr, [](Motor& motor, Ready&, const Enable&)
            {
                return Enabled{ motor.drive };
            }),
        Machine::Row<Enabled, Disable, Ready>(nullptr, [](Motor&, Enabled& from, const Disable&)
            {
                return Ready{ from.data };
            }),
        Machine::InternalRow<Enabled, Setpoint>([](Motor& motor, Enabled&, const Setpoint& event)
            {
                motor.drive.Apply(event.value);
            }),
        Machine::RowFromAny<FaultDetected, Fault>(nullptr, [](Motor&, State&, const FaultDetected& event)
            {
                return Fault{ event.code };
            }),
    };

    return infra::MakeRange(rows);
}
```

| Method                                                  | Meaning                                                                                                                                                                          |
|---------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Row<From, Ev, To>(guard, action)`                      | External transition. Runs `OnExit()` of the source, the action, `OnEntry()` of the target. Without an action the target is default constructed                                   |
| `RowFromAny<Ev, To>(guard, action)`                     | External transition from every state. The guard and action receive the `State` variant. A row for the specific state is consulted first                                          |
| `InternalRow<S, Ev>(action, guard)`                     | The event is handled in the state without leaving it: no exit, no entry, no state change. Without an action the event is accepted and ignored                                    |
| `services::JoinRows(arrays...)`                         | Concatenates `std::array`s of rows, so that a table can be assembled from named groups                                                                                           |
| `WithStorage<QueueDepth>(context, table)`               | The machine with storage for the queue of events dispatched while it is busy                                                                                                     |
| `Start<Initial>(args...)`                               | Checks the table, constructs the initial state, runs its `OnEntry()` and notifies observers                                                                                      |
| `Dispatch(event)`                                       | Handles an event and returns a `services::DispatchResult`                                                                                                                        |
| `OnEntered<S>(hook)`                                    | Registers a captureless `void(Context&, S&)` that runs after `S` has been committed and announced to observers, also for the initial state                                       |
| `Completion<Ev>()`                                      | Returns an `infra::Function<void()>` that dispatches `Ev{}` unless the machine has moved on since                                                                                |
| `CompletionWith<void(Args...)>(mapper)`                 | Returns an `infra::Function<void(Args...)>` that builds an event from the callback arguments with a captureless `mapper` and dispatches it unless the machine has moved on since |
| `CurrentState()`, `CurrentStateId()`, `Is<S>()`         | Inspect the active state                                                                                                                                                         |
| `CheckConsistency<Initial>()`, `HasTransition<S, Ev>()` | Inspect the table                                                                                                                                                                |

Several rows for the same state and event are allowed when all but the last carry a guard; they are consulted in the order of the table and the first row whose guard accepts wins.

## Consistency

`Start()` refuses, with `really_assert`, a table for which `CheckConsistency()` does not return `services::ConsistencyError::none`. The checks are:

| Error                 | Meaning                                                                                             |
|-----------------------|-----------------------------------------------------------------------------------------------------|
| `emptyTable`          | The table has no rows                                                                               |
| `duplicateTransition` | Two unguarded rows for the same state and event                                                     |
| `shadowedTransition`  | A row for a state and event follows an unguarded row for the same pair, so it can never be selected |
| `unreachableState`    | A state class of the variant that no sequence of rows reaches from the initial state                |

Because the set of states is the variant itself, every state class must take part in the graph.

## Validating a table

The checks are implemented by `services::TransitionTableAnalysis<Machine>`, which `Start()` uses as well. The analysis is `constexpr` and needs no machine and no context. When the rows are returned by a `static constexpr` function, such as a public `Motor::Rows()` from which `Table()` builds its range, the compiler verifies the table and a broken table fails the build instead of asserting on the target:

```cpp
using Analysis = services::TransitionTableAnalysis<Motor::Machine>;

static_assert(Analysis(Motor::Rows()).IsValid(Analysis::StateId::Of<Idle>()));
static_assert(Analysis(Motor::Rows()).IsValid(Analysis::StateId::Of<Idle>(), Analysis::Terminal<Fault>(), services::Severity::warning));
```

The analysis also accepts a `Machine::Table`, for instance the range returned by `Transitions()`. Besides the consistency errors above, it reports findings that do not stop the machine from starting, but usually point at a mistake:

| Finding               | Severity | Meaning                                                                                                   |
|-----------------------|----------|-----------------------------------------------------------------------------------------------------------|
| `emptyTable`          | error    | See above                                                                                                 |
| `duplicateTransition` | error    | See above; reports both rows                                                                              |
| `shadowedTransition`  | error    | See above; reports both rows                                                                              |
| `unreachableState`    | error    | See above; reports every unreachable state                                                                |
| `unusedEvent`         | warning  | An event class of the variant that no row handles, so it is forbidden in every state                      |
| `deadEndState`        | warning  | A state that no external row leaves. States listed with `Analysis::Terminal<S...>()` are exempt           |
| `overriddenAnyRow`    | warning  | A row built with `RowFromAny` for which every state has an unguarded specific row, so that it never fires |
| `canReject`           | info     | A state and event for which every applicable row is guarded, so that `Dispatch()` may return `rejected`   |

| Method                                          | Meaning                                                                                    |
|-------------------------------------------------|--------------------------------------------------------------------------------------------|
| `CheckConsistency(initial)`                     | The first consistency error, as `Start()` checks it                                        |
| `IsValid(initial, terminal, failAt)`            | Whether no finding has severity `failAt` or higher; `failAt` defaults to `error`           |
| `HighestSeverity(initial, terminal)`            | The severity of the most severe finding, if any                                            |
| `ForEachFinding(initial, terminal, minimum, f)` | Calls `f` for each finding of severity `minimum` or higher, with its rows, state and event |

`services::WriteValidationReport(stream, analysis, initial, terminal, minimum)` writes the findings as text, one per line, naming the rows by their index and their states and events by name:

```
error shadowedTransition: row 1 (Closed --Push--> Open [guarded]) follows unguarded row 0 (Closed --Push--> Open) and is never selected
warning deadEndState: Jammed has no transition to another state
info canReject: Pull in Open is rejected when every guard refuses
```

The report does not allocate, so it can be written to a tracer on the target as well.

## Validation tool

A `static_assert` needs the rows of the table where the assertion is written. For tables that are private to a component, or to validate all state machines of a project in one step and generate their diagrams, `application/fsm_validator` provides a host command line tool. A table cannot be discovered at runtime, so the tool is a `main` that is linked with a registration source per project:

```cpp
#include "application/fsm_validator/FsmRegistry.hpp"
#include "motor/Motor.hpp"

namespace
{
    constexpr auto motorRows = Motor::Rows();

    application::FsmRegistrationFor<Motor::Machine, Idle> motor{ "Motor", infra::MakeRange(motorRows) };
    application::FsmRegistrationFor<Upgrade::Machine, Idle, Done> upgrade{ "Upgrade", Upgrade::Table() };
}
```

The template arguments are the machine, the initial state and the terminal states, if any. The CMake function `emil_add_fsm_validator` builds the tool for host builds and registers it with `ctest`, so that a broken table fails the test run:

```cmake
emil_add_fsm_validator(motor.fsm_validator
    SOURCES FsmRegistrations.cpp
    LINK_LIBRARIES motor
    STRICT
    MERMAID_DIR ${CMAKE_CURRENT_SOURCE_DIR}/doc
)
```

`STRICT` fails on warnings as well as on errors. `MERMAID_DIR` adds a target `motor.fsm_validator.mermaid` that writes a `<name>.mmd` diagram per machine. The tool can also be run by hand:

| Option            | Meaning                                                   |
|-------------------|-----------------------------------------------------------|
| `names...`        | Validate only the named state machines; all when omitted  |
| `--strict`        | Fail on warnings as well as on errors                     |
| `--info`          | Also report informational findings                        |
| `--list`          | List the registered state machines                        |
| `--mermaid <dir>` | Write a `<name>.mmd` diagram per state machine to `<dir>` |

```
$ motor.fsm_validator --info
[Motor]
info canReject: Enable in Ready is rejected when every guard refuses
[Motor] passed
[Upgrade]
[Upgrade] passed
```

The exit code is non-zero when a state machine fails or an unknown name is given.

## Dispatching events

`Dispatch()` selects the first applicable row and returns one of four results.
`transitioned` means a row was executed.
`forbidden` means no row exists for the event in the current state;
`rejected` means rows exist but every guard refused.
In both cases the state is unchanged and observers are notified, but nothing asserts: whether a forbidden event is a programming error or a normal occurrence, such as a command received over a bus in the wrong state, is for the owner to decide.
`queued` means `Dispatch()` was called while the machine was already handling an event, from an action, an entry or exit method, an observer or an entered hook.
Such an event is stored in the queue and handled after the current transition has fully committed and been notified, so every action always observes a consistent machine.
A full queue asserts.

A transition is committed in a fixed order: `OnExit()` of the source state, the action which builds the target state object, replacement of the state, `OnEntry()` of the target state, notification of observers, and finally the hook registered with `OnEntered<S>()` for the target state. Side effects that must see the new state belong in `OnEntry()` when the state class can carry what they need.
They belong in an `OnEntered<S>()` hook when they need the context, such as starting a drive the owner holds or completing a command callback the owner stores.
An event dispatched from either is queued like any other nested dispatch.

## Asynchronous completions

An action that starts an asynchronous operation passes `fsm.Completion<Ev>()` as its completion callback.
The returned function dispatches `Ev` when invoked, but only when the machine has not transitioned since the callback was created.
A completion that arrives after the machine has left the state that started the operation, for instance because a fault occurred in the meantime, is discarded and reported to observers as `EventDiscarded`.
The check compares an epoch that every transition advances, not the state, so a completion from an earlier run of the same state is discarded as well.
When the service reports a result, `fsm.CompletionWith<void(Result)>([](Result result) { return Saved{ result }; })` builds the event from the callback arguments with a captureless mapper and applies the same rule.
This replaces the manual generation counters and "am I still in the right state" checks that asynchronous state machines otherwise need in every callback.
The owning object must outlive the callback, as for every other callback in this library; objects managed by shared pointers wrap the completion in the usual `infra::WeakPtr` construction.

## Observers

`services::TableStateMachine` is an `infra::Subject` for `services::StateMachineObserver<State, Event>`, which reports `Started`, `StateChanged`, `EventHandled` for internal rows, `EventForbidden`, `EventRejected` and `EventDiscarded`. Any number of observers may attach. Three helpers are provided:

- `services::StateMachineTracer` writes every transition, every internally handled event and every forbidden, rejected or discarded event to a `services::Tracer`, for instance `fsm: Idle --Calibrate--> Calibrating`, `fsm: handled Setpoint in Enabled` and `fsm: forbidden Enable in Idle`.
- `services::StateTimeouts` holds a table of `services::StateTimeout` rows, each naming a state, a duration and an event. When the state becomes active a single-shot timer is started; when it expires the event is dispatched; leaving the state cancels the timer.
- `services::WriteMermaid` is not an observer but a function that writes the transition table of a machine or of a `services::TransitionTableAnalysis` as a `stateDiagram-v2` block, so that documentation can be generated from the table, or a test can compare the table with a diagram kept in the documentation. A row built with `RowFromAny` is drawn as one edge per state, except from states in which an unguarded specific row for the same event always wins.

```cpp
constexpr std::array<services::StateTimeout<State, Event>, 1> timeouts{ {
    { services::AlternativeId<State>::Of<Calibrating>(), std::chrono::seconds(10), Event{ Timeout{} } },
} };

services::StateMachineTracer<State, Event> stateMachineTracer{ fsm, tracer };
services::StateTimeouts<State, Event> stateTimeouts{ fsm, infra::MakeRange(timeouts) };
```

## Execution context

The machine holds no locks and runs entirely on the context that calls `Dispatch()`, which is expected to be the event dispatcher.
Reading the state from another context, such as an interrupt handler, while the dispatcher may be transitioning is a data race, so the state is only inspected from the dispatcher context as well.
An interrupt handler that must react before the event dispatcher runs does its immediate work in the interrupt, on data it owns, and schedules the dispatch of the corresponding event on `infra::EventDispatcher`.

## Testing

`services/fsm/test_doubles` provides `services::StateMachineObserverMock` and `services::StateMachineTester`.
The tester drives a machine to a state along a declared path of events and, given one sample of every event class, dispatches every event in every state and checks that the machine forbids an event exactly when the table has no row for it.
This one test replaces the hand-written list of "command X is rejected in state Y" tests that a state machine otherwise accumulates.
Because the expectation is derived from the table itself, it verifies that the machine and the component behave as the table says, for instance that no action or entry method dispatches unexpectedly; it does not detect that a row was added or removed on purpose. Which events a component accepts in which state is a requirement of the component, and is best tested directly on the component's own interface.
The table itself is verified with `services::TransitionTableAnalysis`, see [Validating a table](#validating-a-table).

`services/fsm/test/JobLifecycle.hpp` contains a complete example component: an asynchronous job that prepares, commits its settings and runs, with timeouts, guarded rows, internal rows, rows from any state and completions. It is exercised by `TestStateMachineIntegration.cpp` and validated by the `application.fsm_validator_example` tool.
