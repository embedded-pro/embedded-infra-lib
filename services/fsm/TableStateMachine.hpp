#ifndef SERVICES_TABLE_STATE_MACHINE_HPP
#define SERVICES_TABLE_STATE_MACHINE_HPP

#include "infra/util/BoundedDeque.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "services/fsm/StateMachine.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace services
{
    enum class ConsistencyError : uint8_t
    {
        none,
        emptyTable,
        duplicateTransition,
        shadowedTransition,
        unreachableState
    };

    template<class T>
    concept HasOnEntry = requires(T& state) { state.OnEntry(); };

    template<class T>
    concept HasOnExit = requires(T& state) { state.OnExit(); };

    template<class T>
    concept Captureless = std::is_empty_v<T> && std::is_default_constructible_v<T>;

    template<class T, std::size_t... N>
    constexpr std::array<T, (N + ... + 0)> JoinRows(const std::array<T, N>&... parts);

    template<class State, class Event, class Context>
    class TableStateMachine
        : public StateMachine<State, Event>
    {
    public:
        using StateId = AlternativeId<State>;
        using EventId = AlternativeId<Event>;

        struct Transition
        {
            std::optional<std::size_t> from;
            std::size_t event;
            std::size_t to;
            bool internal;
            bool guarded;
            bool (*guard)(TableStateMachine& machine, const Event& event);
            void (*execute)(TableStateMachine& machine, const Event& event);
        };

        using Table = infra::MemoryRange<const Transition>;

        template<std::size_t QueueDepth>
        class WithStorage;

        TableStateMachine(Context& context, Table table, infra::BoundedDeque<Event>& queue);

        template<class From, class Ev, class To, class Guard = std::nullptr_t, class Action = std::nullptr_t>
        static constexpr Transition Row(Guard guard = nullptr, Action action = nullptr);
        template<class Ev, class To, class Guard = std::nullptr_t, class Action = std::nullptr_t>
        static constexpr Transition RowFromAny(Guard guard = nullptr, Action action = nullptr);
        template<class S, class Ev, class Action = std::nullptr_t, class Guard = std::nullptr_t>
        static constexpr Transition InternalRow(Action action = nullptr, Guard guard = nullptr);

        template<class Initial, class... Args>
        void Start(Args&&... args);
        bool Started() const;

        template<class Initial>
        ConsistencyError CheckConsistency() const;
        ConsistencyError CheckConsistency(StateId initial) const;
        template<class S, class E>
        bool HasTransition() const;
        bool HasTransition(StateId from, EventId event) const;
        Table Transitions() const;

        const State& CurrentState() const override;
        DispatchResult Dispatch(const Event& event) override;
        bool Dispatching() const;

        template<class S, class Hook>
        TableStateMachine& OnEntered(Hook hook);

        template<class E, std::size_t ExtraSize = INFRA_DEFAULT_FUNCTION_EXTRA_SIZE>
        infra::Function<void(), ExtraSize> Completion();
        template<class E, std::size_t ExtraSize = INFRA_DEFAULT_FUNCTION_EXTRA_SIZE>
        infra::Function<void(), ExtraSize> Completion(E event);
        template<class Signature, class Mapper, std::size_t ExtraSize = INFRA_DEFAULT_FUNCTION_EXTRA_SIZE>
        infra::Function<Signature, ExtraSize> CompletionWith(Mapper mapper);

    private:
        template<class Signature>
        struct CompletionSignature;

        template<class... Args>
        struct CompletionSignature<void(Args...)>
        {
            template<class Mapper, std::size_t ExtraSize>
            static infra::Function<void(Args...), ExtraSize> Make(TableStateMachine& machine, uint32_t epochAtRequest);
        };

        template<class From, class Ev, class Guard>
        static bool Guarded(TableStateMachine& machine, const Event& event);
        template<class From, class Ev, class To, class Action>
        static void Executed(TableStateMachine& machine, const Event& event);
        template<class S, class Ev, class Action>
        static void ExecutedInternal(TableStateMachine& machine, const Event& event);
        template<class S, class Hook>
        static void Entered(TableStateMachine& machine);
        template<class From>
        static auto& Source(State& state);

        template<class From, class Ev, class To, class Action>
        void Execute(const Event& event);
        template<class To, class From, class Ev, class Action>
        To Build(From& from, const Ev& event);
        void RunExit();
        void RunEntry();

        DispatchResult DispatchNow(const Event& event);
        const Transition* Select(const Event& event, bool& rowExists);
        const Transition* SelectFrom(std::optional<std::size_t> from, const Event& event, bool& rowExists);
        DispatchResult Reject(const Event& event, bool rowExists);
        void DrainQueue();
        void CompleteWith(const Event& event, uint32_t epochAtRequest);
        void RunEnteredHook();

        bool HasDuplicate() const;
        bool HasShadowed() const;
        bool HasUnreachable(StateId initial) const;
        static bool SamePair(const Transition& a, const Transition& b);

    private:
        Context& context;
        Table table;
        infra::BoundedDeque<Event>& queue;
        std::optional<State> currentState;
        std::array<void (*)(TableStateMachine&), StateId::count> enteredHooks{};
        uint32_t epoch{ 0 };
        bool dispatching{ false };
    };

    template<class State, class Event, class Context>
    template<std::size_t QueueDepth>
    class TableStateMachine<State, Event, Context>::WithStorage
        : public TableStateMachine<State, Event, Context>
    {
    public:
        WithStorage(Context& context, Table table);

    private:
        typename infra::BoundedDeque<Event>::template WithMaxSize<QueueDepth> queueStorage;
    };

    ////    Implementation    ////

    template<class T, std::size_t... N>
    constexpr std::array<T, (N + ... + 0)> JoinRows(const std::array<T, N>&... parts)
    {
        std::array<T, (N + ... + 0)> result{};
        std::size_t index = 0;

        auto append = [&result, &index](const auto& part)
        {
            for (const auto& row : part)
                result[index++] = row;
        };
        (append(parts), ...);

        return result;
    }

    template<class State, class Event, class Context>
    TableStateMachine<State, Event, Context>::TableStateMachine(Context& context, Table table, infra::BoundedDeque<Event>& queue)
        : context(context)
        , table(table)
        , queue(queue)
    {}

    template<class State, class Event, class Context>
    template<std::size_t QueueDepth>
    TableStateMachine<State, Event, Context>::WithStorage<QueueDepth>::WithStorage(Context& context, Table table)
        : TableStateMachine<State, Event, Context>(context, table, queueStorage)
    {}

    template<class State, class Event, class Context>
    template<class From, class Ev, class To, class Guard, class Action>
    constexpr typename TableStateMachine<State, Event, Context>::Transition TableStateMachine<State, Event, Context>::Row(Guard, Action)
    {
        return Transition{ StateId::template Of<From>().Index(), EventId::template Of<Ev>().Index(), StateId::template Of<To>().Index(), false, !std::is_null_pointer_v<Guard>, &Guarded<From, Ev, Guard>, &Executed<From, Ev, To, Action> };
    }

    template<class State, class Event, class Context>
    template<class Ev, class To, class Guard, class Action>
    constexpr typename TableStateMachine<State, Event, Context>::Transition TableStateMachine<State, Event, Context>::RowFromAny(Guard, Action)
    {
        return Transition{ std::nullopt, EventId::template Of<Ev>().Index(), StateId::template Of<To>().Index(), false, !std::is_null_pointer_v<Guard>, &Guarded<State, Ev, Guard>, &Executed<State, Ev, To, Action> };
    }

    template<class State, class Event, class Context>
    template<class S, class Ev, class Action, class Guard>
    constexpr typename TableStateMachine<State, Event, Context>::Transition TableStateMachine<State, Event, Context>::InternalRow(Action, Guard)
    {
        return Transition{ StateId::template Of<S>().Index(), EventId::template Of<Ev>().Index(), StateId::template Of<S>().Index(), true, !std::is_null_pointer_v<Guard>, &Guarded<S, Ev, Guard>, &ExecutedInternal<S, Ev, Action> };
    }

    template<class State, class Event, class Context>
    template<class Initial, class... Args>
    void TableStateMachine<State, Event, Context>::Start(Args&&... args)
    {
        really_assert(!Started());
        really_assert(CheckConsistency<Initial>() == ConsistencyError::none);

        dispatching = true;
        currentState.emplace(std::in_place_type<Initial>, std::forward<Args>(args)...);
        ++epoch;
        RunEntry();
        this->NotifyObservers([this](StateMachineObserver<State, Event>& observer)
            {
                observer.Started(this->CurrentStateId());
            });
        RunEnteredHook();
        DrainQueue();
        dispatching = false;
    }

    template<class State, class Event, class Context>
    bool TableStateMachine<State, Event, Context>::Started() const
    {
        return currentState.has_value();
    }

    template<class State, class Event, class Context>
    template<class Initial>
    ConsistencyError TableStateMachine<State, Event, Context>::CheckConsistency() const
    {
        return CheckConsistency(StateId::template Of<Initial>());
    }

    template<class State, class Event, class Context>
    ConsistencyError TableStateMachine<State, Event, Context>::CheckConsistency(StateId initial) const
    {
        if (table.empty())
            return ConsistencyError::emptyTable;
        if (HasDuplicate())
            return ConsistencyError::duplicateTransition;
        if (HasShadowed())
            return ConsistencyError::shadowedTransition;
        if (HasUnreachable(initial))
            return ConsistencyError::unreachableState;

        return ConsistencyError::none;
    }

    template<class State, class Event, class Context>
    template<class S, class E>
    bool TableStateMachine<State, Event, Context>::HasTransition() const
    {
        return HasTransition(StateId::template Of<S>(), EventId::template Of<E>());
    }

    template<class State, class Event, class Context>
    bool TableStateMachine<State, Event, Context>::HasTransition(StateId from, EventId event) const
    {
        for (const auto& transition : table)
            if ((!transition.from || *transition.from == from.Index()) && transition.event == event.Index())
                return true;

        return false;
    }

    template<class State, class Event, class Context>
    typename TableStateMachine<State, Event, Context>::Table TableStateMachine<State, Event, Context>::Transitions() const
    {
        return table;
    }

    template<class State, class Event, class Context>
    const State& TableStateMachine<State, Event, Context>::CurrentState() const
    {
        really_assert(Started());
        return *currentState;
    }

    template<class State, class Event, class Context>
    DispatchResult TableStateMachine<State, Event, Context>::Dispatch(const Event& event)
    {
        really_assert(Started());

        if (dispatching)
        {
            really_assert(!queue.full());
            queue.push_back(event);
            return DispatchResult::queued;
        }

        dispatching = true;
        auto result = DispatchNow(event);
        DrainQueue();
        dispatching = false;
        return result;
    }

    template<class State, class Event, class Context>
    bool TableStateMachine<State, Event, Context>::Dispatching() const
    {
        return dispatching;
    }

    template<class State, class Event, class Context>
    template<class S, class Hook>
    TableStateMachine<State, Event, Context>& TableStateMachine<State, Event, Context>::OnEntered(Hook)
    {
        really_assert(!Started());
        enteredHooks[StateId::template Of<S>().Index()] = &Entered<S, Hook>;
        return *this;
    }

    template<class State, class Event, class Context>
    template<class E, std::size_t ExtraSize>
    infra::Function<void(), ExtraSize> TableStateMachine<State, Event, Context>::Completion()
    {
        static_assert(std::is_default_constructible_v<E>, "Completion events without payload must be default constructible");
        return [this, epochAtRequest = epoch]()
        {
            CompleteWith(Event{ E{} }, epochAtRequest);
        };
    }

    template<class State, class Event, class Context>
    template<class E, std::size_t ExtraSize>
    infra::Function<void(), ExtraSize> TableStateMachine<State, Event, Context>::Completion(E event)
    {
        return [this, epochAtRequest = epoch, event]()
        {
            CompleteWith(Event{ event }, epochAtRequest);
        };
    }

    template<class State, class Event, class Context>
    template<class Signature, class Mapper, std::size_t ExtraSize>
    infra::Function<Signature, ExtraSize> TableStateMachine<State, Event, Context>::CompletionWith(Mapper)
    {
        static_assert(Captureless<Mapper>, "Completion mapper must be a captureless callable");
        return CompletionSignature<Signature>::template Make<Mapper, ExtraSize>(*this, epoch);
    }

    template<class State, class Event, class Context>
    template<class... Args>
    template<class Mapper, std::size_t ExtraSize>
    infra::Function<void(Args...), ExtraSize> TableStateMachine<State, Event, Context>::CompletionSignature<void(Args...)>::Make(TableStateMachine& machine, uint32_t epochAtRequest)
    {
        return [&machine, epochAtRequest](Args... args)
        {
            machine.CompleteWith(Event{ Mapper{}(args...) }, epochAtRequest);
        };
    }

    template<class State, class Event, class Context>
    template<class From, class Ev, class Guard>
    bool TableStateMachine<State, Event, Context>::Guarded(TableStateMachine& machine, const Event& event)
    {
        if constexpr (std::is_null_pointer_v<Guard>)
            return true;
        else
        {
            static_assert(Captureless<Guard>, "Guards must be captureless; the context is passed as their first argument");
            static_assert(std::is_invocable_r_v<bool, Guard, Context&, const From&, const Ev&>, "Guard must be callable as bool(Context&, const From&, const Ev&)");
            return Guard{}(machine.context, std::as_const(Source<From>(*machine.currentState)), std::get<Ev>(event));
        }
    }

    template<class State, class Event, class Context>
    template<class From, class Ev, class To, class Action>
    void TableStateMachine<State, Event, Context>::Executed(TableStateMachine& machine, const Event& event)
    {
        machine.template Execute<From, Ev, To, Action>(event);
    }

    template<class State, class Event, class Context>
    template<class S, class Ev, class Action>
    void TableStateMachine<State, Event, Context>::ExecutedInternal(TableStateMachine& machine, const Event& event)
    {
        if constexpr (!std::is_null_pointer_v<Action>)
        {
            static_assert(Captureless<Action>, "Actions must be captureless; the context is passed as their first argument");
            static_assert(std::is_invocable_v<Action, Context&, S&, const Ev&>, "Internal action must be callable as void(Context&, S&, const Ev&)");
            Action{}(machine.context, std::get<S>(*machine.currentState), std::get<Ev>(event));
        }
    }

    template<class State, class Event, class Context>
    template<class S, class Hook>
    void TableStateMachine<State, Event, Context>::Entered(TableStateMachine& machine)
    {
        static_assert(Captureless<Hook>, "Entered hooks must be captureless; the context is passed as their first argument");
        static_assert(std::is_invocable_v<Hook, Context&, S&>, "Entered hook must be callable as void(Context&, S&)");
        Hook{}(machine.context, std::get<S>(*machine.currentState));
    }

    template<class State, class Event, class Context>
    template<class From>
    auto& TableStateMachine<State, Event, Context>::Source(State& state)
    {
        if constexpr (std::is_same_v<From, State>)
            return state;
        else
            return std::get<From>(state);
    }

    template<class State, class Event, class Context>
    template<class From, class Ev, class To, class Action>
    void TableStateMachine<State, Event, Context>::Execute(const Event& event)
    {
        RunExit();
        ++epoch;
        To next{ this->template Build<To, From, Ev, Action>(Source<From>(*currentState), std::get<Ev>(event)) };
        currentState->template emplace<To>(std::move(next));
        RunEntry();
    }

    template<class State, class Event, class Context>
    template<class To, class From, class Ev, class Action>
    To TableStateMachine<State, Event, Context>::Build(From& from, const Ev& event)
    {
        if constexpr (std::is_null_pointer_v<Action>)
        {
            static_assert(std::is_default_constructible_v<To>, "Target state must be default constructible when no action builds it");
            return To{};
        }
        else
        {
            static_assert(Captureless<Action>, "Actions must be captureless; the context is passed as their first argument");
            static_assert(std::is_invocable_r_v<To, Action, Context&, From&, const Ev&>, "Action must be callable as To(Context&, From&, const Ev&)");
            return Action{}(context, from, event);
        }
    }

    template<class State, class Event, class Context>
    void TableStateMachine<State, Event, Context>::RunExit()
    {
        std::visit([](auto& state)
            {
                if constexpr (HasOnExit<std::remove_cvref_t<decltype(state)>>)
                    state.OnExit();
            },
            *currentState);
    }

    template<class State, class Event, class Context>
    void TableStateMachine<State, Event, Context>::RunEntry()
    {
        std::visit([](auto& state)
            {
                if constexpr (HasOnEntry<std::remove_cvref_t<decltype(state)>>)
                    state.OnEntry();
            },
            *currentState);
    }

    template<class State, class Event, class Context>
    DispatchResult TableStateMachine<State, Event, Context>::DispatchNow(const Event& event)
    {
        bool rowExists = false;
        const Transition* selected = Select(event, rowExists);

        if (selected == nullptr)
            return Reject(event, rowExists);

        auto from = this->CurrentStateId();
        selected->execute(*this, event);

        if (selected->internal)
            this->NotifyObservers([&](StateMachineObserver<State, Event>& observer)
                {
                    observer.EventHandled(from, event);
                });
        else
        {
            this->NotifyObservers([&](StateMachineObserver<State, Event>& observer)
                {
                    observer.StateChanged(from, event, this->CurrentStateId());
                });
            RunEnteredHook();
        }

        return DispatchResult::transitioned;
    }

    template<class State, class Event, class Context>
    const typename TableStateMachine<State, Event, Context>::Transition* TableStateMachine<State, Event, Context>::Select(const Event& event, bool& rowExists)
    {
        const Transition* specific = SelectFrom(currentState->index(), event, rowExists);
        if (specific != nullptr)
            return specific;

        return SelectFrom(std::nullopt, event, rowExists);
    }

    template<class State, class Event, class Context>
    const typename TableStateMachine<State, Event, Context>::Transition* TableStateMachine<State, Event, Context>::SelectFrom(std::optional<std::size_t> from, const Event& event, bool& rowExists)
    {
        for (const auto& transition : table)
            if (transition.from == from && transition.event == event.index())
            {
                rowExists = true;
                if (!transition.guarded || transition.guard(*this, event))
                    return &transition;
            }

        return nullptr;
    }

    template<class State, class Event, class Context>
    DispatchResult TableStateMachine<State, Event, Context>::Reject(const Event& event, bool rowExists)
    {
        auto state = this->CurrentStateId();

        if (rowExists)
        {
            this->NotifyObservers([&](StateMachineObserver<State, Event>& observer)
                {
                    observer.EventRejected(state, event);
                });
            return DispatchResult::rejected;
        }

        this->NotifyObservers([&](StateMachineObserver<State, Event>& observer)
            {
                observer.EventForbidden(state, event);
            });
        return DispatchResult::forbidden;
    }

    template<class State, class Event, class Context>
    void TableStateMachine<State, Event, Context>::DrainQueue()
    {
        while (!queue.empty())
        {
            Event event{ std::move(queue.front()) };
            queue.pop_front();
            DispatchNow(event);
        }
    }

    template<class State, class Event, class Context>
    void TableStateMachine<State, Event, Context>::CompleteWith(const Event& event, uint32_t epochAtRequest)
    {
        if (epochAtRequest == epoch)
            Dispatch(event);
        else
            this->NotifyObservers([&](StateMachineObserver<State, Event>& observer)
                {
                    observer.EventDiscarded(this->CurrentStateId(), event);
                });
    }

    template<class State, class Event, class Context>
    void TableStateMachine<State, Event, Context>::RunEnteredHook()
    {
        auto hook = enteredHooks[currentState->index()];

        if (hook != nullptr)
            hook(*this);
    }

    template<class State, class Event, class Context>
    bool TableStateMachine<State, Event, Context>::HasDuplicate() const
    {
        for (auto first = table.begin(); first != table.end(); ++first)
            for (auto second = first + 1; second != table.end(); ++second)
                if (SamePair(*first, *second) && !first->guarded && !second->guarded)
                    return true;

        return false;
    }

    template<class State, class Event, class Context>
    bool TableStateMachine<State, Event, Context>::HasShadowed() const
    {
        for (auto first = table.begin(); first != table.end(); ++first)
            for (auto second = first + 1; second != table.end(); ++second)
                if (SamePair(*first, *second) && !first->guarded)
                    return true;

        return false;
    }

    template<class State, class Event, class Context>
    bool TableStateMachine<State, Event, Context>::HasUnreachable(StateId initial) const
    {
        std::array<bool, StateId::count> reached{};
        reached[initial.Index()] = true;

        bool changed = true;
        while (changed)
        {
            changed = false;
            for (const auto& transition : table)
                if (!reached[transition.to] && (!transition.from || reached[*transition.from]))
                {
                    reached[transition.to] = true;
                    changed = true;
                }
        }

        for (bool state : reached)
            if (!state)
                return true;

        return false;
    }

    template<class State, class Event, class Context>
    bool TableStateMachine<State, Event, Context>::SamePair(const Transition& a, const Transition& b)
    {
        return a.from == b.from && a.event == b.event;
    }
}

#endif
