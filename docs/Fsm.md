# Finite State Machine

## Introduction

Many components in an embedded application are state machines: a connection is closed, connecting or connected; a motor is idle, calibrating, ready or enabled; a firmware upgrade is downloading, verifying or installing.
Writing such a component by hand tends to scatter the rules over many methods: every command checks the current state, every asynchronous callback checks it again, and the set of forbidden transitions exists only in the head of the author.
The `services/fsm` package provides a generic, heap-less state machine in which the transition table is the single source of truth.
The table is checked for consistency before the machine starts, every event that is not allowed in the current state is reported rather than silently ignored, and asynchronous completions cannot corrupt the machine when they arrive late.

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

`services::TableStateMachine<State, Event>` is instantiated with storage for a maximum number of transitions and for the queue of events dispatched while the machine is busy, and is filled with rows before it is started.
Guards and actions are callables, typically lambdas capturing the owning object.
A guard receives the current state object and the event and returns whether the row applies; an action receives the current state object and the event and returns the new state object.

```cpp
services::TableStateMachine<State, Event>::WithStorage<16, 4> fsm;

fsm.Add<Idle, Calibrate, Calibrating>(nullptr, [this](Idle&, const Calibrate&)
       {
           calibration.Start(fsm.Completion<CalibrationDone>());
           return Calibrating{};
       })
    .Add<Calibrating, CalibrationDone, Ready>([this](const Calibrating&, const CalibrationDone&)
        {
            return calibration.Result().has_value();
        },
        [this](Calibrating&, const CalibrationDone&)
        {
            return Ready{ *calibration.Result() };
        })
    .Add<Calibrating, CalibrationDone, Idle>()
    .Add<Ready, Enable, Enabled>(nullptr, [this](Ready&, const Enable&)
        {
            return Enabled{ drive };
        })
    .Add<Enabled, Disable, Ready>(nullptr, [](Enabled& from, const Disable&)
        {
            return Ready{ from.data };
        })
    .AddInternal<Enabled, Setpoint>([this](Enabled&, const Setpoint& event)
        {
            drive.Apply(event.value);
        })
    .AddFromAny<FaultDetected, Fault>(nullptr, [](State&, const FaultDetected& event)
        {
            return Fault{ event.code };
        });

fsm.Start<Idle>();
```

| Method                                                  | Meaning                                                                                                                                        |
|---------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------|
| `Add<From, Ev, To>(guard, action)`                      | External transition. Runs `OnExit()` of the source, the action, `OnEntry()` of the target. Without an action the target is default constructed |
| `AddFromAny<Ev, To>(guard, action)`                     | External transition from every state. The guard and action receive the `State` variant. A row for the specific state is consulted first        |
| `AddInternal<S, Ev>(action, guard)`                     | The event is handled in the state without leaving it: no exit, no entry, no notification. Without an action the event is accepted and ignored  |
| `Start<Initial>(args...)`                               | Checks the table, constructs the initial state, runs its `OnEntry()` and notifies observers                                                    |
| `Dispatch(event)`                                       | Handles an event and returns a `services::DispatchResult`                                                                                      |
| `Completion<Ev>()`                                      | Returns an `infra::Function<void()>` that dispatches `Ev{}` unless the machine has moved on since                                              |
| `CurrentState()`, `CurrentStateId()`, `Is<S>()`         | Inspect the active state                                                                                                                       |
| `CheckConsistency<Initial>()`, `HasTransition<S, Ev>()` | Inspect the table                                                                                                                              |

Several rows for the same state and event are allowed when all but the last carry a guard; they are consulted in the order they were added and the first row whose guard accepts wins.

## Consistency

`Start()` refuses, with `really_assert`, a table for which `CheckConsistency()` does not return `services::ConsistencyError::none`. The checks are:

| Error                 | Meaning                                                                                             |
|-----------------------|-----------------------------------------------------------------------------------------------------|
| `emptyTable`          | No rows were added                                                                                  |
| `duplicateTransition` | Two unguarded rows for the same state and event                                                     |
| `shadowedTransition`  | A row for a state and event follows an unguarded row for the same pair, so it can never be selected |
| `unreachableState`    | A state class of the variant that no sequence of rows reaches from the initial state                |

Because the set of states is the variant itself, every state class must take part in the graph. A unit test of a component that owns a state machine normally asserts `CheckConsistency` explicitly, so that a broken table is reported by name instead of by an assertion at start.

## Dispatching events

`Dispatch()` selects the first applicable row and returns one of four results.
`transitioned` means a row was executed.
`forbidden` means no row exists for the event in the current state;
`rejected` means rows exist but every guard refused.
In both cases the state is unchanged and observers are notified, but nothing asserts: whether a forbidden event is a programming error or a normal occurrence, such as a command received over a bus in the wrong state, is for the owner to decide.
`queued` means `Dispatch()` was called while the machine was already handling an event, from an action, an entry or exit method, or an observer.
Such an event is stored in the queue and handled after the current transition has fully committed and been notified, so every action always observes a consistent machine.
A full queue asserts.

A transition is committed in a fixed order: `OnExit()` of the source state, the action which builds the target state object, replacement of the state, `OnEntry()` of the target state, notification of observers. Side effects that must see the new state belong in `OnEntry()`.

## Asynchronous completions

An action that starts an asynchronous operation passes `fsm.Completion<Ev>()` as its completion callback.
The returned function dispatches `Ev` when invoked, but only when the machine has not transitioned since the callback was created.
A completion that arrives after the machine has left the state that started the operation, for instance because a fault occurred in the meantime, is discarded and reported to observers as `EventDiscarded`.
This replaces the manual generation counters and "am I still in the right state" checks that asynchronous state machines otherwise need in every callback.
The owning object must outlive the callback, as for every other callback in this library; objects managed by shared pointers wrap the completion in the usual `infra::WeakPtr` construction.

## Observers

`services::TableStateMachine` is an `infra::Subject` for `services::StateMachineObserver<State, Event>`, which reports `Started`, `StateChanged`, `EventForbidden`, `EventRejected` and `EventDiscarded`. Any number of observers may attach. Three observers are provided:

- `services::StateMachineTracer` writes every transition and every forbidden, rejected or discarded event to a `services::Tracer`, for instance `fsm: Idle --Calibrate--> Calibrating` and `fsm: forbidden Enable in Idle`.
- `services::StateTimeouts` holds a table of `services::StateTimeout` rows, each naming a state, a duration and an event. When the state becomes active a single-shot timer is started; when it expires the event is dispatched; leaving the state cancels the timer.
- `services::WriteMermaid` is not an observer but a function that writes the transition table as a `stateDiagram-v2` block, so that documentation can be generated from the table, or a test can compare the table with a diagram kept in the documentation.

```cpp
constexpr std::array<services::StateTimeout<State, Event>, 1> timeouts{ {
    { services::AlternativeId<State>::Of<Calibrating>(), std::chrono::seconds(10), Event{ Timeout{} } },
} };

services::StateMachineTracer<State, Event> stateMachineTracer{ fsm, tracer };
services::StateTimeouts<State, Event> stateTimeouts{ fsm, infra::MakeRange(timeouts) };
```

## Execution context

The machine holds no locks and runs entirely on the context that calls `Dispatch()`, which is expected to be the event dispatcher.
An interrupt handler that must react before the event dispatcher runs does its immediate work in the interrupt and schedules the dispatch of the corresponding event on `infra::EventDispatcher`; it may read `CurrentStateId()` at any time, since the index of a `std::variant` is a single load.

## Testing

`services/fsm/test_doubles` provides `services::StateMachineObserverMock` and `services::StateMachineTester`.
The tester drives a machine to a state along a declared path of events and, given one sample of every event class, dispatches every event in every state and checks that the machine forbids an event exactly when the table has no row for it.
This one test replaces the hand-written list of "command X is rejected in state Y" tests that a state machine otherwise accumulates, and fails as soon as a row is added or removed without updating the expectations of the component.
