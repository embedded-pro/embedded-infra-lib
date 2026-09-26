# Execution Model

## Execution by using an event dispatcher

Embedded Infrastructure Library supports various execution models, but the most important execution model is execution by scheduling blocks of work on an event dispatcher. This is a lightweight way of being able to perform multiple tasks in parallel, without needing multiple stacks or synchronization.

The event dispatcher is used by scheduling an action by using its `Schedule()` method. `Schedule()` takes a single argument of type `infra::Function<void()>`, and is therefore often used in combination with lambda functions. `Schedule()` does not execute its action immediately, but pushes it on a queue for later execution. The event dispatcher is typically constantly running on the application's main thread, and executes queued actions one after another. This way, a scheduled action is completely finished before the next action is started, so in contrast with running actions in different threads no synchronization is needed.

An action executed by an event dispatcher should never waste any time; no action should ever sleep, waiting for an external task, such as a write towards flash, to finish. Instead, after having started a write towards flash, a new action is scheduled when the write has been finished. In the meantime, other actions can execute. This creates a decoupling between various different pieces of business logic: while one piece is busy with writing to flash, another piece may be answering an HTTP request, or reading out a temperature sensor. No real-time behaviour is guaranteed on the event dispatcher (any real-time behaviour required is implemented outside of event dispatchers, by either interrupt handlers or by using threads), but multiple components can flawlessly share execution time, with each component making steady progress.

While it is possible to have multiple event dispatchers (when using multiple threads, a specific thread may have its own event dispatcher for dedicated tasks), there is one event dispatcher globally available via `infra::EventDispatcher::Instance()`. This event dispatcher is used for generic tasks; for example, the `ConfigurationStore` uses the `Flash` interface to read and write towards flash; the completion of a read or write action is dispatched on this globally available event dispatcher. A typical `main` function will therefore start with declaring an event dispatcher, and end with invoking that event dispatcher's `Run()` method.

### Support for infra::WeakPtr

Objects that are managed by shared pointers may schedule actions to be executed, but it may happen that that object is destroyed before that action is executed. In that case, that action should be discarded instead of being executed. `infra::EventDispatcherWithWeakPtr` exists to facilitate this usecase. Next to the typical `Schedule()` function that takes an action as parameter, it supports an overload that takes a second parameter: an `infra::WeakPtr<T>` towards the object that schedules the action. That parameter is converted to a shared pointer just before executing the action: if this conversion succeeds, the object is still alive, and the action is executed with that shared pointer as parameter. If the conversion fails, then the object was already expired, and execution of that action is skipped.

## Multi-threaded execution

While currently no component in Embedded Infrastructure Library requires the usage of multiple threads, it is perfectly viable to use an operating system to start more than one thread. A reason to use multiple threads include having a dedicated thread for executing tasks that require real-time behaviour. By separating execution of the event dispatcher and execution of time-critical tasks, enough execution time can be guaranteed for real-time behaviour.

Another reason for using multiple threads is when a certain action takes a considerable amount of time, for instance generating a certificate takes multiple seconds. Executing that action on the main event dispatcher would delay the execution of other actions; a solution to this could be to generate the certificate in its own thread, and its completion can be scheduled on the main event dispatcher.

A thread may have its own event dispatcher. This removes the need for starting and stopping a thread; when a thread-aware event dispatcher is idle, it will pause its thread, and it will wake up its thread when new work is scheduled.

## Execution without an event dispatcher

Some applications do not benefit from having an event dispatcher. A boot loader's main focus is to be small and execute one thing; it only loads an application and therefore has no need to execute multiple actions in parallel. The small overhead that an event dispatcher brings does not bring any benefits, and should therefore not be needed in a boot loader. For this kind of usecase, variations of the interfaces for interacting with peripherals exist which work synchronously; they do not need to schedule their completion on an event dispatcher, but they complete their activities before returning. While for other libraries this is often the default behaviour, for Embedded Infrastructure Library this is the exception.

A number of components in Embedded Infrastructure Library assume the presence of an event dispatcher. This includes any component that makes use of an asynchronous interface. Obviously, without an event dispatcher such components cannot be used.

## Idling in low power

When the event dispatcher runs out of work it calls `Idle()`. `infra::LowPowerEventDispatcher` forwards that call to an `infra::LowPowerStrategy`, which decides how the processor waits for the next interrupt.

On Cortex-M, `hal::cortex::LowPowerStrategyWithModes` masks interrupts, checks that the event dispatcher is still idle, and then asks a `hal::LowPowerMode` to enter `hal::PowerMode::sleep` or `hal::PowerMode::deepSleep`. The core wakes on any pending interrupt, even with interrupts masked, so work scheduled from an interrupt between the idle check and the wait is never missed.

Deep sleep is chosen only when both of these hold:

- No component holds the `infra::MainClockReference`. Peripherals that need the main clock while a transfer is in progress, such as `services::LowPowerSpiMaster` and `services::LowPowerSerialCommunication`, hold it for that time.
- No timer is pending. The system tick stops in deep sleep, so a pending timer would be delayed until some other interrupt wakes the core.

`hal::LowPowerMode` is implemented per vendor, because deep sleep requires vendor-specific clock configuration.

### Preparing for and resuming from deep sleep

Work around deep sleep falls in two groups.

Short, synchronous work that must happen right before and right after each deep sleep, such as switching pins to analog or notifying a radio, goes in a `hal::cortex::DeepSleepObserver` attached to the strategy. `EnteringDeepSleep()` and `LeftDeepSleep()` are called only around deep sleep, not around sleep, with interrupts masked. They must not block, but they may schedule work on the event dispatcher. Vendor implementations restore the run-mode clocks before `LeftDeepSleep()` is called.

Asynchronous work, such as putting an external sensor or flash into its low-power state over SPI or I2C, cannot run with interrupts masked. It runs on the event dispatcher before deep sleep is allowed, coordinated by an `infra::SystemStateManager`. Every component that has to prepare is a `infra::SystemStateParticipant`: it starts its preparation when a state is requested, and calls `ReachedState()` when it has finished. The manager requests the next state only after all participants reached the current one. The application holds the `infra::MainClockReference` while running, and releases it in a final state, after all participants have prepared:

```cpp
struct StatePrepareForDeepSleep : infra::SystemState<StatePrepareForDeepSleep> {};
struct StateReadyForDeepSleep : infra::SystemState<StateReadyForDeepSleep> {};

class PowerManager
    : public infra::SystemStateParticipant
{
public:
    PowerManager(infra::SystemStateManager& manager, infra::MainClockReference& mainClock)
        : infra::SystemStateParticipant(manager)
        , mainClock(mainClock)
    {
        mainClock.Refere();
    }

protected:
    void RequestState(infra::SystemStateBase state) override
    {
        if (state == StateReadyForDeepSleep())
            mainClock.Release();

        ReachedState();
    }

private:
    infra::MainClockReference& mainClock;
};
```

Resuming mirrors this: the interrupt that woke the device schedules a run of the states that restore the participants, and the application takes the `MainClockReference` again.

## Supervising the event dispatcher with a watchdog

A stuck event dispatcher does not stop interrupts, so refreshing a hardware watchdog from an interrupt alone does not detect it. `services::EventDispatcherWatchdogWorker<Worker>` extends any event dispatcher worker with supervision by a `hal::Watchdog`:

- The worker counts a step when an action starts and when it finishes, so an odd count means an action is executing.
- The watchdog raises an early-warning interrupt every `EarlyWarningPeriod()`, and the worker refreshes it from there. If the dispatcher is idle, or its steps advanced since the previous early warning, it is making progress. Otherwise the same action is still executing, and the early warning counts as missed.
- Once the missed early warnings cover the expiration timeout, `onExpired` is called from the interrupt so it can record why the device resets. After that the watchdog is no longer refreshed, so it resets the device even when `onExpired` returns.

`services::EventDispatcherWithWatchdog`, `services::EventDispatcherWithWeakPtrAndWatchdog` and `services::LowPowerEventDispatcherWithWeakPtrAndWatchdog` are ready-made combinations. They take the watchdog, the expiration timeout and `onExpired` before the arguments of the dispatcher they extend:

```cpp
services::LowPowerEventDispatcherWithWeakPtrAndWatchdog::WithSize<50> eventDispatcher(watchdog, std::chrono::milliseconds(1500), onExpired, lowPowerStrategy);
```

No timer is involved, so an idle dispatcher can enter deep sleep while it is supervised. Code that has to keep interrupts disabled for longer than the early-warning period, such as a flash erase, calls `Refresh()` on the watchdog directly.
