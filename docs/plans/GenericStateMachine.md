# Plan: generic finite state machine in `services/fsm`

Status: proposal, awaiting answers to the open questions at the end.

## 1. Overview

### Goal

Add a generic, heap-less, event-driven finite state machine (FSM) library to EmIL under
`services/fsm`, namespace `services`, that:

- describes a machine declaratively in one transition table that is the single source of truth;
- validates that table at compile time (duplicate, shadowed and unreachable transitions, dangling
  entry/exit behaviours, malformed internal transitions) so an inconsistent machine does not build;
- detects and reports every forbidden or guard-rejected event at run time, to the caller and to
  observers, without asserting;
- runs to completion: an event dispatched from inside an action is queued and handled after the
  current transition has fully committed;
- gives asynchronous completions a first-class, stale-safe way back into the machine;
- is fully testable in isolation (unit) and in composition with the event dispatcher, timers and
  tracer (integration), and ships the test doubles and helpers that make an application's own FSM
  tests short.

### Why a table-driven design and not the existing `PolymorphicVariant` idiom

EmIL already has two hand-rolled FSM idioms: polymorphic state objects in an
`infra::PolymorphicVariant` (`services/sesame/SesameWindowed.hpp:65`,
`services/crypto/EllipticCurveDiffieHellman.hpp:18`) and `std::variant` states with `Transition()`
plus a `Handle()` overload per state (`services/flash/FlashGeometrySfdpBase.hpp:34`). Both are
good for protocol parsers, but neither exposes the transition graph as data, so nothing can check
it, trace it, diagram it or enumerate the forbidden pairs. That is exactly what the e-foc state
machines lack today: the legal graph lives only in `documentation/design/state-machine.md`, guards
are fused into command methods (`FocStateMachineCommon.cpp:91-108`), the forbidden matrix is
transcribed by hand into roughly thirty `*_is_rejected` tests per mode, and the "re-check state in
every async callback" rule is a convention rather than a mechanism. The table-driven design fixes
each of these. The `PolymorphicVariant` idiom stays where it is; this library is for machines whose
graph must be verifiable.

### What the library provides

| Component | Header | Role |
|---|---|---|
| `Transition<Owner, State, Event>` | `services/fsm/Transition.hpp` | One table row: from, event, to, optional guard, optional action, external/internal |
| `StateBehaviour<Owner, State>` | `services/fsm/Transition.hpp` | Per-state entry and exit actions |
| `anyState` | `services/fsm/Transition.hpp` | Wildcard source, for "fault from every state" |
| `TransitionTable<Owner, State, Event>` | `services/fsm/TransitionTable.hpp` | Validated, immutable view over static tables; only constructible through `Validate()` |
| `CheckConsistency()`, `ConsistencyError` | `services/fsm/TransitionTable.hpp` | `constexpr` analysis returning the first error found |
| `Validate()` | `services/fsm/TransitionTable.hpp` | `consteval` wrapper that turns any error into a compile error |
| `StateMachine<State, Event>` | `services/fsm/StateMachine.hpp` | Abstract subject: `CurrentState()`, `Dispatch()`; observable |
| `StateMachineObserver<State, Event>` | `services/fsm/StateMachine.hpp` | `Started`, `StateChanged`, `EventForbidden`, `EventRejected`, `EventDiscarded` |
| `DispatchResult` | `services/fsm/StateMachine.hpp` | `transitioned`, `rejected`, `forbidden`, `queued` |
| `TableStateMachine<Owner, State, Event>` | `services/fsm/TableStateMachine.hpp` | The engine; `WithEventQueue<N>` supplies the run-to-completion queue |
| `StateMachineTracer<State, Event>` | `services/fsm/StateMachineTracer.hpp` | Observer that writes transitions and rejections to a `services::Tracer` |
| `StateTimeouts<State, Event>` | `services/fsm/StateTimeouts.hpp` | Observer that dispatches an event when a state has been active for a duration |
| `WriteMermaid()` | `services/fsm/StateMachineMermaid.hpp` | Writes the table as a `stateDiagram-v2`, to keep documentation in sync |
| `StateMachineObserverMock` | `services/fsm/test_doubles/StateMachineObserverMock.hpp` | GoogleMock observer |
| `StateMachineTester` | `services/fsm/test_doubles/StateMachineTester.hpp` | Exhaustive state x event matrix driver |

Affected modules: `services` (new library), `docs` (new `docs/Fsm.md`), `README.md`, `CLAUDE.md`,
`services/CMakeLists.txt`. About 22 new files, 4 modified. No existing code changes behaviour.

## 2. Design

### 2.1 State and event types

`State` and `Event` are application `enum class` types with a fixed-width underlying type. The
library `static_assert`s both are enums whose underlying type is at most 16 bits so a state fits in
one load, which is what lets an interrupt handler read `CurrentState()` without locking. Data that
belongs to a state (e-foc's `Calibrating::pendingData`, `Ready::rotorReferenceValid`) is owned by
the owner class, not by the state: the machine tracks which state is active, the owner keeps what
that state needs. This is a deliberate departure from e-foc's `std::variant<Idle, Calibrating, ...>`
and is open question 2.

### 2.2 Transition table

```cpp
namespace services
{
    inline constexpr struct AnyState {} anyState;

    enum class TransitionKind : uint8_t
    {
        external,
        internal
    };

    template<class Owner, class State, class Event>
    struct Transition
    {
        using Guard = bool (Owner::*)() const;
        using Action = void (Owner::*)();

        constexpr Transition(State from, Event event, State to, Guard guard = nullptr, Action action = nullptr);
        constexpr Transition(AnyState, Event event, State to, Guard guard = nullptr, Action action = nullptr);
        static constexpr Transition Internal(State state, Event event, Action action, Guard guard = nullptr);

        std::optional<State> from;
        Event event;
        State to;
        Guard guard;
        Action action;
        TransitionKind kind;
    };

    template<class Owner, class State>
    struct StateBehaviour
    {
        State state;
        void (Owner::*onEntry)();
        void (Owner::*onExit)();
    };
}
```

Guards and actions are member function pointers of the owning class rather than `infra::Function`
objects. `infra::Function` is not a literal type, so a table of them could not be `constexpr` and
could not be validated at compile time; member pointers are literal, cost one word each, allocate
nothing and keep the table as static, read-only data. Lambdas therefore cannot appear in the table;
an action that needs a lambda calls it from a named member function. This is open question 7.

Because a static data member initializer is not a complete-class context, `&Owner::PrivateMethod`
cannot be taken inside the class body. The documented pattern is a static member function that owns
the table as a function-local `static constexpr` object, which is a complete-class context and has
access to private members:

```cpp
const services::TransitionTable<MotorLifecycle, State, Event>& MotorLifecycle::Table()
{
    static constexpr std::array transitions{
        services::Transition<MotorLifecycle, State, Event>{ State::idle, Event::calibrate, State::calibrating, &MotorLifecycle::NoCommandPending, &MotorLifecycle::StartCalibration },
        services::Transition<MotorLifecycle, State, Event>{ State::calibrating, Event::calibrationSucceeded, State::ready },
        services::Transition<MotorLifecycle, State, Event>{ State::ready, Event::enable, State::enabled, &MotorLifecycle::RotorReferenceValid },
        services::Transition<MotorLifecycle, State, Event>{ services::anyState, Event::fault, State::fault },
        services::Transition<MotorLifecycle, State, Event>::Internal(State::enabled, Event::setpoint, &MotorLifecycle::ApplySetpoint)
    };

    static constexpr std::array behaviours{
        services::StateBehaviour<MotorLifecycle, State>{ State::enabled, &MotorLifecycle::StartDrive, &MotorLifecycle::StopDrive },
        services::StateBehaviour<MotorLifecycle, State>{ State::fault, &MotorLifecycle::LatchFault, nullptr }
    };

    static constexpr auto table = services::Validate(transitions, behaviours, State::idle);
    return table;
}
```

`Validate` is `consteval`, so the table is analysed while compiling this translation unit and the
initializer of `table` is a constant. `TransitionTable` holds `infra::MemoryRange<const Transition>`
and `infra::MemoryRange<const StateBehaviour>` into the two static arrays plus the initial state.

### 2.3 Compile-time consistency checks

`constexpr ConsistencyError CheckConsistency(transitions, behaviours, initial)` returns the first
violation found, in this order:

| `ConsistencyError` | Meaning |
|---|---|
| `none` | Table is consistent |
| `emptyTable` | No transitions |
| `duplicateTransition` | Two rows with the same source and event and no guard on either |
| `shadowedTransition` | A row for (source, event) follows an unguarded row for the same pair, so it can never fire |
| `internalTransitionChangesState` | An internal row whose target differs from its source |
| `duplicateBehaviour` | Two `StateBehaviour` rows for one state |
| `behaviourForUnknownState` | A `StateBehaviour` for a state that no row and not the initial state mentions |
| `unreachableState` | A state mentioned as a source or target that no path from the initial state reaches, ignoring guards and treating `anyState` rows as edges from every known state |

The state set is collected from the initial state and every row's source and target into a
`std::array` bounded by `2 * transitions.size() + 1`, so the analysis needs no allocation and no
sentinel enumerator. Reachability is an iterative worklist, not recursion.

`consteval TransitionTable Validate(...)` calls `CheckConsistency` and, on any error, calls a
non-`constexpr` function named after the error (`ConsistencyViolation::DuplicateTransition()` and
so on). Calling a non-`constexpr` function inside a `consteval` evaluation is ill-formed, so the
compiler's diagnostic names the violated rule. Applications that want a friendlier message can add
`static_assert(services::CheckConsistency(transitions, behaviours, State::idle) == services::ConsistencyError::none)`
next to the table; the tests do both.

Not checked, on purpose: guard semantics (guards are opaque), and states with no outgoing rows
(terminal states are legitimate). Both are open question 5.

### 2.4 Run-time semantics of `TableStateMachine`

```cpp
namespace services
{
    enum class DispatchResult : uint8_t
    {
        transitioned,
        rejected,
        forbidden,
        queued
    };

    template<class Owner, class State, class Event>
    class TableStateMachine
        : public StateMachine<State, Event>
    {
    public:
        template<std::size_t QueueDepth>
        class WithEventQueue;

        TableStateMachine(Owner& owner, const TransitionTable<Owner, State, Event>& table, infra::BoundedDeque<Event>& queue);

        void Start();
        State CurrentState() const override;
        DispatchResult Dispatch(Event event) override;
        infra::Function<void()> Completion(Event event);
        bool Dispatching() const;

    private:
        const Transition<Owner, State, Event>* Select(Event event) const;
        DispatchResult Execute(const Transition<Owner, State, Event>& transition, Event event);
        void RunExit(State state);
        void RunEntry(State state);
        void DrainQueue();

        Owner& owner;
        const TransitionTable<Owner, State, Event>& table;
        infra::BoundedDeque<Event>& queue;
        State currentState;
        uint32_t epoch{ 0 };
        bool started{ false };
        bool dispatching{ false };
    };
}
```

- `Start()` runs the initial state's entry action and notifies `Started(initial)`. Nothing runs
  from the constructor, matching `FlashGeometrySfdpBase.cpp:9-13`. `Dispatch` before `Start` is a
  `really_assert`.
- `Select(event)` scans rows in table order, first those whose source equals the current state,
  then `anyState` rows, and returns the first whose guard is absent or returns true. A specific row
  always beats a wildcard row. No row for the pair at all is `forbidden`; rows exist but every guard
  refused is `rejected`. Both leave the state untouched and notify `EventForbidden` or
  `EventRejected`. Neither asserts; the caller maps the result to its own status enum.
- `Execute` commits in this order: exit action of the source (external rows only), then
  `currentState = to` and `++epoch`, then the row's action, then entry action of the target
  (external rows only), then `StateChanged(from, event, to)`. The state is committed before any
  side effect, which is the ordering e-foc had to hand-write in `EnterEnabled` and `EnterFault` so a
  fault raised during `Start()` is never overwritten.
- An external self-transition (`from == to`) runs exit and entry; an internal row runs only its
  action. An event that must be accepted and ignored in some state is declared as an internal row
  with no action, so that "not in the table" always means forbidden.
- `Dispatch` while `dispatching` is true pushes the event onto the queue and returns `queued`; the
  outer `Dispatch` drains the queue after notifying observers, one event at a time, so each nested
  event sees a fully committed state. A full queue is a `really_assert`, because dropping events
  silently is worse than a visible failure. Queue depth comes from `WithEventQueue<N>`.
- `Completion(event)` returns an `infra::Function<void()>` that captures `this`, `event` and the
  current `epoch`. When invoked it dispatches the event only if `epoch` is unchanged; otherwise it
  notifies `EventDiscarded(currentState, event)` and does nothing. An action that starts an
  asynchronous operation passes `fsm.Completion(Event::calibrationSucceeded)` as the callback and
  gets e-foc's `if (aborted || token != runToken) return;` preamble for free. The function fits in
  the default `infra::Function` storage. Lifetime of the owner across the callback is the owner's
  responsibility, as for every other callback in EmIL; where the owner is shared-pointer managed the
  owner wraps the completion with `infra::WeakPtr` as usual.
- Execution context: all of the above runs on whichever context calls `Dispatch`, expected to be the
  event dispatcher. The library holds no locks. An interrupt that must react before the dispatcher
  runs (e-foc's `PlatformFaultNotifier`) keeps doing its immediate work in the interrupt and
  schedules `Dispatch` on the dispatcher; the interrupt may read `CurrentState()`.

### 2.5 Observation

```cpp
namespace services
{
    template<class State, class Event>
    class StateMachine;

    template<class State, class Event>
    class StateMachineObserver
        : public infra::Observer<StateMachineObserver<State, Event>, StateMachine<State, Event>>
    {
    public:
        using infra::Observer<StateMachineObserver<State, Event>, StateMachine<State, Event>>::Observer;

        virtual void Started(State initial)
        {}

        virtual void StateChanged(State from, Event event, State to) = 0;

        virtual void EventForbidden(State state, Event event)
        {}

        virtual void EventRejected(State state, Event event)
        {}

        virtual void EventDiscarded(State state, Event event)
        {}
    };

    template<class State, class Event>
    class StateMachine
        : public infra::Subject<StateMachineObserver<State, Event>>
    {
    public:
        virtual State CurrentState() const = 0;
        virtual DispatchResult Dispatch(Event event) = 0;
    };
}
```

`StateMachine<State, Event>` is independent of `Owner`, so tracers, timeouts, CAN bridges and test
mocks depend only on the state and event enums. The subject is multi-observer
(`infra::Subject` with the `void` helper) because a tracer, a timeout table and the application
typically all listen. This replaces e-foc's single-slot `readyHandler` and its four external
`holds_alternative` classifiers with one notification carrying from, event and to.

### 2.6 Tracing, timeouts, diagrams

`StateMachineTracer<State, Event>` is an observer constructed with the machine and a
`services::Tracer&`. It requires `infra::TextOutputStream& operator<<(infra::TextOutputStream&, State)`
and the same for `Event`, found by argument-dependent lookup, which is how EmIL prints enums
everywhere else. Output lines are `fsm: idle --calibrate--> calibrating`,
`fsm: forbidden enable in idle`, `fsm: rejected enable in ready` and
`fsm: discarded calibrationSucceeded in fault`.

`StateTimeouts<State, Event>` is an observer holding one `infra::TimerSingleShot` and an
`infra::MemoryRange<const StateTimeout<State, Event>>` of `{ state, infra::Duration, event }`.
On `Started` and `StateChanged` it cancels the timer and, if the new state has a row, starts it with
an action that dispatches the row's event. Leaving the state cancels; re-entering restarts. e-foc
has no timeouts today, so this is new capability and open question 8.

`WriteMermaid(infra::TextOutputStream&, const TransitionTable&)` emits a `stateDiagram-v2` block
using the same `operator<<` overloads, with guards rendered as `[guarded]` and internal rows as
`state --> state : event (internal)`. It exists so a test can regenerate the diagram that e-foc's
documentation rules require and fail when the checked-in diagram drifts. Open question 9.

## 3. Detailed steps

Each step lists the file, the action and what it contains. Steps are ordered so the tests of a step
compile against the headers of the same step (TDD: write the test, watch it fail, implement).

### Phase 1: core engine

| # | File | Action | Content |
|---|---|---|---|
| 1 | `services/CMakeLists.txt` | Modify | `add_subdirectory(fsm)` in the unguarded block after `flash` |
| 2 | `services/fsm/CMakeLists.txt` | Create | `add_library(services.fsm ${EMIL_EXCLUDE_FROM_ALL} STATIC)`, links `infra.util`, `infra.event`, `infra.timer`, `services.tracer`; sources listed alphabetically; `add_subdirectory(test)`, `add_subdirectory(test_doubles)` |
| 3 | `services/fsm/Transition.hpp` | Create | `AnyState`, `anyState`, `TransitionKind`, `Transition`, `StateBehaviour`; header guard `SERVICES_FSM_TRANSITION_HPP` |
| 4 | `services/fsm/TransitionTable.hpp` | Create | `ConsistencyError`, `CheckConsistency`, `ConsistencyViolation` (non-`constexpr` named functions), `TransitionTable` with `Transitions()`, `Behaviours()`, `Initial()`, `HasTransition(State, Event)`, `Behaviour(State)`; `Validate` |
| 5 | `services/fsm/StateMachine.hpp` | Create | `DispatchResult`, `StateMachineObserver`, `StateMachine` |
| 6 | `services/fsm/TableStateMachine.hpp` | Create | `TableStateMachine` and nested `WithEventQueue<N>` owning `infra::BoundedDeque<Event>::WithMaxSize<N>` |
| 7 | `services/fsm/test_doubles/CMakeLists.txt` | Create | `services.fsm_test_doubles` INTERFACE, `emil_build_for(... BOOL BUILD_TESTING)`, links `gmock`, `services.fsm` |
| 8 | `services/fsm/test_doubles/StateMachineObserverMock.hpp` | Create | `MOCK_METHOD` for all five observer callbacks |
| 9 | `services/fsm/test/CMakeLists.txt` | Create | `services.fsm_test`, `emil_build_for(... BOOL EMIL_BUILD_TESTS)` then `emil_add_test`, links `gmock_main`, `services.fsm`, `services.fsm_test_doubles`, `infra.event_test_helper`, `infra.timer_test_helper`, `infra.util_test_helper` |
| 10 | `services/fsm/test/TestTransitionTable.cpp` | Create | Consistency tests (section 4.1) |
| 11 | `services/fsm/test/TestTableStateMachine.cpp` | Create | Engine tests (section 4.2) |
| 12 | `docs/Fsm.md` | Create | Introduction, contrast with `infra::Sequencer` and `PolymorphicVariant`, API table, examples lifted from the tests, execution-context rules |
| 13 | `README.md`, `CLAUDE.md` | Modify | Add `docs/Fsm.md` to both documentation lists |

All `services/fsm` headers are templates, so the library has no `.cpp` in phase 1; `STATIC` with
header-only sources is the existing convention and the Darwin archiver flags in the root
`CMakeLists.txt` already cover the empty archive.

### Phase 2: observation helpers and integration tests

| # | File | Action | Content |
|---|---|---|---|
| 14 | `services/fsm/StateMachineTracer.hpp` | Create | Observer writing to `services::Tracer` |
| 15 | `services/fsm/test/TestStateMachineTracer.cpp` | Create | Exact trace lines through `services::TracerToStream` on an `infra::StringOutputStream` |
| 16 | `services/fsm/test_doubles/StateMachineTester.hpp` | Create | `ForEachStateAndEvent(table, allStates, allEvents, callback)` and `DriveTo(machine, path)`; the exhaustive forbidden-matrix driver |
| 17 | `services/fsm/test/TestStateMachineIntegration.cpp` | Create | The reference `DeviceLifecycle` machine on `infra::ClockFixture` (section 4.4) |

### Phase 3: timeouts

| # | File | Action | Content |
|---|---|---|---|
| 18 | `services/fsm/StateTimeouts.hpp` | Create | `StateTimeout` row, `StateTimeouts` observer |
| 19 | `services/fsm/test/TestStateTimeouts.cpp` | Create | Timer behaviour (section 4.3) |

### Phase 4: documentation export

| # | File | Action | Content |
|---|---|---|---|
| 20 | `services/fsm/StateMachineMermaid.hpp` | Create | `WriteMermaid` |
| 21 | `services/fsm/test/TestStateMachineMermaid.cpp` | Create | Golden output for the reference machine |

### Phase 5: adopt in e-foc (separate repository, separate PR, after an EmIL release)

Not part of this EmIL change, but the proof that the library is generic enough:

1. Bump the EmIL pin in e-foc.
2. Replace `state_machine::State` (`FocStateMachine.hpp:43`) with
   `enum class State : uint8_t { idle, calibrating, ready, enabled, fault }` and introduce
   `enum class Event : uint8_t { calibrate, calibrationSucceeded, calibrationFailed, enable, disable,
   clearFault, fault, emergencyStop, clearCalibration, calibrationCleared, bootCalibrationValid,
   bootCalibrationInvalid, reAlign, alignmentDone, externalCalibrationReserved,
   externalCalibrationCompleted }`.
3. Move state payloads into `FocStateMachineCommon` members: `pendingData`, `loadedData`,
   `rotorReferenceValid`, `faultCode`, `calibrationStep`.
4. Express the graph from `documentation/design/state-machine.md:63-87` as the table; the four
   copies of `!IsStopped(currentState) || HasPendingCommand()` become one guard; `Stop()` and
   `ResetClearCount()` become the exit behaviour of `enabled`; `fault` and `emergencyStop` become
   `anyState` rows; `Cmd*` methods shrink to `return ToCommandResult(fsm.Dispatch(Event::x))`.
5. Replace the generation-token preambles in `CalibrationOrchestrator` and the NVM callbacks with
   `fsm.Completion(...)`.
6. Make `FocMotorCanBridge`, `CanLivenessWatchdog` and `ControlHealth` observers instead of pollers.
7. Replace the six near-identical fixtures and the transcribed rejection tests with one
   `StateMachineTester` matrix per mode; add a test that regenerates the mermaid diagram and compares
   it with the documentation.
8. Delete `TransitionPolicies.hpp` and the stale `E_FOC_AUTO_TRANSITION_POLICY` paragraph.

## 4. Test strategy (written before the implementation)

Framework: GoogleTest and GoogleMock, `testing::StrictMock<>` only, no heap, `{}` initialization,
`TEST(ComponentTest, snake_case_behaviour)` or a fixture where the dispatcher or clock is needed.
Death tests for every `really_assert`.

The test owner is a class whose guards and actions are `MOCK_METHOD`s; member pointers to mocked
methods are ordinary member pointers, so the table references them directly and `testing::InSequence`
verifies ordering.

### 4.1 `TestTransitionTable.cpp`

- `valid_table_has_no_consistency_error`: `static_assert` and `EXPECT_EQ` on `ConsistencyError::none`.
- One test per `ConsistencyError` value building the smallest table that triggers it:
  `empty_table_is_rejected`, `two_unguarded_rows_for_same_pair_are_duplicate`,
  `row_after_unguarded_row_is_shadowed`, `guarded_rows_for_same_pair_are_allowed`,
  `internal_row_with_different_target_is_rejected`, `two_behaviours_for_one_state_are_duplicate`,
  `behaviour_for_state_not_in_table_is_rejected`, `state_not_reachable_from_initial_is_rejected`,
  `any_state_row_makes_target_reachable`, `initial_state_with_no_rows_is_reachable`.
- `has_transition_reports_specific_and_wildcard_rows`, `behaviour_lookup_returns_nullptr_for_state_without_behaviour`.
- `validate_is_usable_as_constant_initializer`: the table from section 2.2 in a
  `static constexpr` local.

### 4.2 `TestTableStateMachine.cpp`

- `start_runs_entry_of_initial_state_and_notifies_started`.
- `dispatch_before_start_asserts` (death test).
- `matching_row_runs_exit_action_entry_then_notifies_in_order`.
- `state_is_committed_before_action_runs`: the action asserts `CurrentState()` is already the target.
- `event_without_row_is_forbidden_and_notifies_without_changing_state`.
- `event_whose_guards_all_refuse_is_rejected_and_notifies`.
- `first_row_whose_guard_accepts_wins`, `guard_is_not_evaluated_for_rows_after_the_winner`.
- `specific_row_beats_any_state_row`, `any_state_row_fires_from_every_state`.
- `internal_row_runs_action_without_exit_or_entry`, `external_self_transition_runs_exit_and_entry`.
- `internal_row_without_action_ignores_event_and_reports_transitioned`.
- `dispatch_from_action_is_queued_and_handled_after_commit`: nested dispatch returns `queued`,
  observers see the outer `StateChanged` first, then the nested one.
- `dispatch_from_observer_is_queued`, `queued_events_are_handled_in_order`,
  `queue_overflow_asserts` (death test).
- `completion_dispatches_when_state_unchanged`,
  `completion_after_transition_is_discarded_and_notifies`,
  `completion_after_self_transition_is_discarded` (epoch, not state, is compared).
- `observer_may_detach_during_notification`.
- `current_state_is_readable_while_dispatching`.

### 4.3 `TestStateTimeouts.cpp` (fixture on `infra::ClockFixture`)

- `entering_state_with_timeout_arms_timer`, `timeout_dispatches_configured_event`,
  `leaving_state_before_timeout_cancels_timer`, `re_entering_state_restarts_timeout`,
  `initial_state_timeout_is_armed_on_start`, `state_without_timeout_does_not_arm_timer`.

### 4.4 `TestStateMachineIntegration.cpp` (fixture on `infra::ClockFixture`)

A reference `DeviceLifecycle` owner, kept in the test, models the shape e-foc needs: idle,
calibrating, ready, enabled, fault; an asynchronous calibration service mock whose completion is
`fsm.Completion(...)`; a storage mock whose save completion is also a completion; `fault` and
`emergencyStop` as `anyState` rows; a `StateMachineTracer` on a `TracerToStream`; a
`StateTimeouts` row that fails calibration after ten seconds.

- `boot_to_enabled_happy_path_traces_every_transition`: exact trace text.
- `calibration_completion_after_fault_is_discarded`: the stale-callback race, now a library guarantee.
- `calibration_timeout_moves_to_idle`.
- `fault_from_any_state_runs_enabled_exit_action_only_when_enabled`.
- `emergency_stop_result_depends_on_source_state`: the owner records the source in the exit actions.
- `forbidden_matrix_matches_table`: `StateMachineTester::ForEachStateAndEvent` drives a fresh
  machine to every state through a declared path, dispatches every event and expects `forbidden`
  exactly when `HasTransition` is false. This single test replaces e-foc's transcribed
  `*_is_rejected` tests.
- `machine_is_driven_only_from_event_dispatcher`: completions scheduled through
  `infra::EventDispatcher::Instance().Schedule` arrive after `ExecuteAllActions()`, never inline.

### 4.5 `TestStateMachineTracer.cpp` and `TestStateMachineMermaid.cpp`

- One test per line format; `mermaid_output_matches_golden_for_reference_machine`.

## 5. Build integration

- `services/CMakeLists.txt`: `add_subdirectory(fsm)`.
- New targets: `services.fsm` (STATIC), `services.fsm_test` (EMIL_BUILD_TESTS),
  `services.fsm_test_doubles` (INTERFACE, BUILD_TESTING).
- No new options, no host-only guard: the library must build for the ARM presets.
- Commands: `cmake --preset host`, `cmake --build --preset host-Debug`, `ctest --preset host -R services.fsm`,
  and `cmake --preset embedded` to prove the templates compile with `arm-none-eabi-gcc 15.2`.
- PR title `feat: add generic finite state machine as services/fsm` (lowercase subject; release-please
  generates the changelog entry, `CHANGELOG.md` is not edited by hand).

## 6. Verification checklist

- [ ] `CheckConsistency` is `constexpr`, `Validate` is `consteval`, both evaluated in a
      `static constexpr` initializer on GCC, Clang, MSVC and arm-none-eabi-gcc.
- [ ] No `new`, `delete`, `std::function`, `std::vector`, `std::string` anywhere under
      `services/fsm`, tests included.
- [ ] No recursion: reachability is a worklist, queue draining is a loop.
- [ ] `really_assert` only for programming errors (dispatch before start, queue overflow); never for
      forbidden or rejected events.
- [ ] Every function under 30 lines; `Execute` is split into `RunExit`, `RunEntry`, `DrainQueue`.
- [ ] Header guards `SERVICES_FSM_<FILE>_HPP`; `template<class T>`, not `typename`; Allman braces;
      `////    Implementation    ////` separator before template bodies; every override marked.
- [ ] `clang-format` clean (`ColumnLimit: 0`, so no manual wrapping of template signatures).
- [ ] `docs/Fsm.md` examples are copied from passing tests; `README.md` and `CLAUDE.md` link it.
- [ ] Coverage of `services/fsm` at 100 percent of lines on the `coverage` preset.

## 7. Open questions

Each question lists the recommended answer, which the plan above assumes.

1. **Location and namespace.** `services/fsm` with namespace `services` as requested. The library
   depends only on `infra` plus `services::Tracer`, so `infra/fsm` would also be defensible.
   Recommendation: `services/fsm`, namespace `services`, as asked.
2. **State representation.** Enum states with per-state data owned by the owner, versus e-foc's
   `std::variant` of payload-carrying structs. The enum makes the table, the compile-time checks,
   tracing, timeouts and the exhaustive test matrix possible, and keeps `CurrentState()` a single
   load. The cost is that state data is no longer scoped by the type system. Recommendation: enum.
3. **Hierarchical or orthogonal states.** e-foc's `ControlModeStateMachine` selects one of three
   sub-machines, and `IsStopped()` is an ad-hoc super-state. Recommendation: flat machines only in
   this version, composed by an owner that holds a child machine and forwards events from its own
   actions; keep the interface free of anything that would block adding nested states later.
4. **Event payloads.** Events are enums without data; an action that needs data reads it from an
   owner member set before dispatching, which is what e-foc does with `pendingSelectMode`.
   Recommendation: no payloads in this version; revisit with typed events if two adopters need it.
5. **Additional compile-time checks.** Should a state with no outgoing rows be an error (it is often
   a legitimate terminal state), and should every event be required to appear at least once?
   Recommendation: neither; both can be added as opt-in checks.
6. **Forbidden event policy.** Return `forbidden` and notify, never assert, so the owner maps the
   result to its own status. An application wanting a hard failure can `really_assert` on the result.
   Recommendation: as described; no policy parameter.
7. **Guards and actions as member function pointers.** Required for a `constexpr` table; excludes
   lambdas from the table itself. Alternative: `void (*)(Owner&)` free functions, which are also
   literal and allow non-capturing lambdas but need public access. Recommendation: member pointers
   with the `Table()` static member function pattern.
8. **Timeouts in scope.** e-foc has none today. Recommendation: include phase 3, it is small and it
   is how "calibration must finish within N seconds" will be expressed.
9. **Mermaid export in scope.** Recommendation: include phase 4 as a header-only helper; it costs
   nothing on targets that do not instantiate it, and it is what keeps e-foc's documentation honest.
10. **Interrupt context.** The library is dispatcher-context only; interrupts may read
    `CurrentState()` and must schedule `Dispatch`. Is a helper that schedules a dispatch on
    `infra::EventDispatcher` wanted, or does that stay in the application as it does now in
    `PlatformFaultNotifier`? Recommendation: stays in the application.
11. **Adopting in e-foc.** Is phase 5 part of this work, to be started once the EmIL change is
    released, or a separate decision? Recommendation: plan it now, execute it as its own PR in e-foc.
12. **Queue depth default.** `WithEventQueue<N>` forces every user to choose. Recommendation: no
    default; a machine whose actions never dispatch can use `WithEventQueue<1>`.
