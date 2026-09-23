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
    CalibrationData data;
};

using State = std::variant<Idle, Calibrating, Ready, Enabled, Fault>;

struct FaultDetected
{
    static constexpr const char* name{ "FaultDetected" };
    FaultCode code;
};

using Event = std::variant<Calibrate, CalibrationDone, Enable, Disable, Setpoint, FaultDetected>;
```

`services::AlternativeId<Variant>` identifies one alternative of such a variant without holding a value. It is what observers receive, what timeouts are keyed on, and what `CurrentStateId()` returns. `AlternativeId<State>::Of<Enabled>()` names a state class, `id.Is<Enabled>()` tests it and `id.Name()` returns its name.

## The transition table

`services::TableStateMachine<State, Event, Context>` runs a table of rows over a context object, normally the component that owns the machine.
Rows are built with the `constexpr` functions `Row`, `RowFromAny` and `InternalRow`, so that a table declared `static constexpr` is a constant in flash and costs no RAM; the machine itself only holds the current state and a queue for events dispatched while it is busy.
Guards and actions are captureless callables, typically lambdas, that receive the context as their first argument.
A guard receives the context, the current state object and the event and returns whether the row applies; an action receives the context, the current state object and the event and returns the new state object.
Because the callables are captureless they can call whatever the context exposes, including its private members when the table is built inside one of its member functions.

Each state machine is described by a *definition*: a type that names the `Machine`, the `Initial` state, a `constexpr` function `Rows()` that returns the table and a `constexpr` function `Rules()` that declares which transitions the table may make (see [Declaring allowed transitions](#declaring-allowed-transitions)).
The machine can only be constructed from `services::Validated<Definition>()`, which checks the definition at compile time; a component is usually its own definition.

```cpp
class Motor
{
public:
    using Machine = services::TableStateMachine<State, Event, Motor>;
    using Initial = Idle;

    Motor(Calibration& calibration, Drive& drive);

    static constexpr std::array<Machine::Transition, 7> Rows();
    static constexpr Machine::Rules Rules();

private:
    Calibration& calibration;
    Drive& drive;
    Machine::WithStorage<4> fsm;
};

constexpr std::array<Motor::Machine::Transition, 7> Motor::Rows()
{
    return {
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
        Machine::Row<Ready, Enable, Enabled>(nullptr, [](Motor& motor, Ready& from, const Enable&)
            {
                return Enabled{ motor.drive, from.data };
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
}

constexpr Motor::Machine::Rules Motor::Rules()
{
    return Machine::Rules{}
        .Allow<Idle, Calibrating>()
        .Allow<Calibrating, Ready, Idle>()
        .Allow<Ready, Enabled>()
        .Allow<Enabled, Ready>()
        .AllowFromAny<Fault>()
        .Terminal<Fault>();
}

Motor::Motor(Calibration& calibration, Drive& drive)
    : calibration(calibration)
    , drive(drive)
    , fsm(*this, services::Validated<Motor>())
{
    fsm.Start<Idle>();
}
```

| Method                                                  | Meaning                                                                                                                                                                          |
|---------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Row<From, Ev, To>(guard, action)`                      | External transition. Runs `OnExit()` of the source, the action, `OnEntry()` of the target. Without an action the target is default constructed                                   |
| `RowFromAny<Ev, To>(guard, action)`                     | External transition from every state. The guard and action receive the `State` variant. A row for the specific state is consulted first                                          |
| `InternalRow<S, Ev>(action, guard)`                     | The event is handled in the state without leaving it: no exit, no entry, no state change. Without an action the event is accepted and ignored                                    |
| `services::JoinRows(arrays...)`                         | Concatenates `std::array`s of rows, so that a table can be assembled from named groups                                                                                           |
| `WithStorage<QueueDepth>(context, Validated<D>())`      | The machine with storage for the queue of events dispatched while it is busy, built from a validated definition                                                                  |
| `Start<Initial>(args...)`                               | Constructs the initial state, which must be the `Initial` of the definition, runs its `OnEntry()` and notifies observers                                                         |
| `Dispatch(event)`                                       | Handles an event and returns a `services::DispatchResult`                                                                                                                        |
| `OnEntered<S>(hook)`                                    | Registers a captureless `void(Context&, S&)` that runs after `S` has been committed and announced to observers, also for the initial state                                       |
| `Completion<Ev>()`                                      | Returns an `infra::Function<void()>` that dispatches `Ev{}` unless the machine has moved on since                                                                                |
| `CompletionWith<void(Args...)>(mapper)`                 | Returns an `infra::Function<void(Args...)>` that builds an event from the callback arguments with a captureless `mapper` and dispatches it unless the machine has moved on since |
| `CurrentState()`, `CurrentStateId()`, `Is<S>()`         | Inspect the active state                                                                                                                                                         |
| `CheckConsistency<Initial>()`, `HasTransition<S, Ev>()` | Inspect the table                                                                                                                                                                |

Several rows for the same state and event are allowed when all but the last carry a guard; they are consulted in the order of the table and the first row whose guard accepts wins.

## Consistency

`services::Validated<Definition>()` refuses, at compile time, a table with any of the following errors; they are checked again, with `really_assert`, by `Start()` of a machine built from an unchecked table in the engine's own tests. The checks are:

| Error                 | Meaning                                                                                             |
|-----------------------|-----------------------------------------------------------------------------------------------------|
| `emptyTable`          | The table has no rows                                                                               |
| `duplicateTransition` | Two unguarded rows for the same state and event                                                     |
| `shadowedTransition`  | A row for a state and event follows an unguarded row for the same pair, so it can never be selected |
| `unreachableState`    | A state class of the variant that no sequence of rows reaches from the initial state                |

Because the set of states is the variant itself, every state class must take part in the graph.

## Declaring allowed transitions

The consistency checks say nothing about which transitions a component is supposed to make. `Rules()` declares that separately from the rows, as a picture of the state diagram, so that a row that makes any other transition is an error:

| Rule                    | Meaning                                                                                                                      |
|-------------------------|------------------------------------------------------------------------------------------------------------------------------|
| `Allow<From, To...>()`  | The transitions from `From` to each `To` are allowed. The first `Allow` or `AllowFromAny` turns the rules into an allow-list |
| `AllowFromAny<To...>()` | The transitions from every state to each `To` are allowed                                                                    |
| `Forbid<From, To...>()` | The transitions from `From` to each `To` are forbidden, also when `AllowFromAny` allows them                                 |
| `Terminal<S...>()`      | The states are intentionally final, so that no `deadEndState` is reported for them                                           |

The rules concern external transitions only; internal rows never change the state and are always permitted, while an external self-transition `S --> S` must be allowed like any other.
A row built with `RowFromAny` is checked once for every source state, except for states in which an unguarded specific row for the same event always wins.
`Forbid` expresses exceptions such as "a fault is only cleared to Idle": `AllowFromAny<Idle>().Forbid<Fault, Ready, Enabled>()`.

## Mandatory validation

`services::Validated<Definition>()` evaluates `services::TransitionTableAnalysis` at compile time and refuses every finding of severity warning or error, so warnings count as errors.
It returns the `services::ValidatedTable` that `TableStateMachine` and `WithStorage` require; there is no other public way to construct a machine, so every state machine in a product is validated by the build, and nothing of the validation remains in flash or runs at start.

| Finding                | Severity | Meaning                                                                                                   |
|------------------------|----------|-----------------------------------------------------------------------------------------------------------|
| `emptyTable`           | error    | See [Consistency](#consistency)                                                                           |
| `duplicateTransition`  | error    | See [Consistency](#consistency); reports both rows                                                        |
| `shadowedTransition`   | error    | See [Consistency](#consistency); reports both rows                                                        |
| `unreachableState`     | error    | See [Consistency](#consistency); reports every unreachable state                                          |
| `contradictoryRule`    | error    | `Rules()` both allows and forbids the same transition                                                     |
| `forbiddenTransition`  | error    | A row makes a transition that `Rules()` forbids                                                           |
| `disallowedTransition` | error    | A row makes a transition that `Rules()` does not allow                                                    |
| `unusedEvent`          | warning  | An event class of the variant that no row handles, so it is forbidden in every state                      |
| `deadEndState`         | warning  | A state that no external row leaves and that is not declared `Terminal`                                   |
| `overriddenAnyRow`     | warning  | A row built with `RowFromAny` for which every state has an unguarded specific row, so that it never fires |
| `unusedAllowance`      | warning  | `Rules()` allows a transition that no row makes, so the rules and the table have drifted apart            |
| `canReject`            | info     | A state and event for which every applicable row is guarded, so that `Dispatch()` may return `rejected`   |

A violation stops the build with one `static_assert` per finding kind. The message names the finding and the value in the failing comparison locates it, for instance the index of the offending row:

```text
error: static assertion failed: disallowedTransition: a row makes a transition that Rules() does not allow; the value is the index of the row
note: the comparison reduces to '(3 == 18446744073709551615)'
```

The analysis can also be used directly, for instance in a unit test or on a table that is not yet used by a machine:

| Method                                       | Meaning                                                                                     |
|----------------------------------------------|---------------------------------------------------------------------------------------------|
| `CheckConsistency(initial)`                  | The first consistency error                                                                 |
| `IsValid(initial, rules, failAt)`            | Whether no finding has severity `failAt` or higher; `failAt` defaults to `error`            |
| `HighestSeverity(initial, rules)`            | The severity of the most severe finding, if any                                             |
| `ForEachFinding(initial, rules, minimum, f)` | Calls `f` for each finding of severity `minimum` or higher, with its rows, states and event |

```cpp
using Analysis = services::TransitionTableAnalysis<Motor::Machine>;

static_assert(Analysis(Motor::Rows()).IsValid(Analysis::StateId::Of<Idle>(), Motor::Rules(), services::Severity::warning));
```

`services::WriteValidationReport(stream, analysis, initial, rules, minimum)` writes the findings as text, one per line, naming the rows by their index and their states and events by name:

```text
error disallowedTransition: row 3 (Jammed --Pull--> Closed) makes Jammed -> Closed, which the rules do not allow
error shadowedTransition: row 1 (Closed --Push--> Open [guarded]) follows unguarded row 0 (Closed --Push--> Open) and is never selected
warning unusedAllowance: Open -> Open is allowed but no row makes it
info canReject: Pull in Open is rejected when every guard refuses
```

The report does not allocate, so it can be written to a tracer on the target as well.

## Validation tool

The build already refuses an invalid definition. `application/fsm_validator` adds a readable report with the names of rows, states and events, the informational findings, and a diagram per state machine. A table cannot be discovered at runtime, so the tool is a `main` that is linked with a registration source per project:

```cpp
#include "application/fsm_validator/FsmRegistry.hpp"
#include "motor/Motor.hpp"

namespace
{
    application::FsmRegistrationFor<Motor> motor{ "Motor" };
    application::FsmRegistrationFor<Upgrade> upgrade{ "Upgrade" };
}
```

The template argument is the definition.
The registration source only needs the definition's header; when the component's constructor, which calls `Validated<>()`, is defined in a source file rather than inline in the header, the tool also builds for a definition that violates its rules and reports every violation by name, where the compiler stops at the first.
The CMake function `emil_add_fsm_validator` builds the tool for host builds and registers it with `ctest`, so that a broken table fails the test run:

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

```text
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
- `services::WriteMermaid` is not an observer but a function that writes the transition table of a machine or of a `services::TransitionTableAnalysis` as a `stateDiagram-v2` block, so that documentation can be generated from the table, or a test can compare the table with a diagram kept in the documentation.
A row built with `RowFromAny` is drawn as one edge per state, except from states in which an unguarded specific row for the same event always wins.

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
Because the expectation is derived from the table itself, it verifies that the machine and the component behave as the table says, for instance that no action or entry method dispatches unexpectedly; it does not detect that a row was added or removed on purpose.
Which events a component accepts in which state is a requirement of the component, and is best tested directly on the component's own interface.
The table itself is verified by the build, see [Mandatory validation](#mandatory-validation).

The engine's own tests need tables that are deliberately inconsistent, or that start in different states;
`services::UncheckedTableStateMachine<State, Event, Context>::WithStorage<N>` in `services/fsm/test_doubles` constructs a machine from an unchecked table for that purpose and is not meant for product code.
`services/fsm/test/compile_fail` holds sources that are compiled with a deliberate mistake by `ctest`, to prove that such a machine is rejected by the compiler.

`services/fsm/test/JobLifecycle.hpp` contains a complete example component: an asynchronous job that prepares, commits its settings and runs, with timeouts, guarded rows, internal rows, rows from any state and completions. It is exercised by `TestStateMachineIntegration.cpp` and validated by the `application.fsm_validator_example` tool.
