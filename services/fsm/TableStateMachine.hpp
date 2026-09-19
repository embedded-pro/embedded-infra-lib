#ifndef SERVICES_TABLE_STATE_MACHINE_HPP
#define SERVICES_TABLE_STATE_MACHINE_HPP

#include "infra/util/BoundedDeque.hpp"
#include "infra/util/BoundedVector.hpp"
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

#ifndef SERVICES_FSM_FUNCTION_EXTRA_SIZE
#define SERVICES_FSM_FUNCTION_EXTRA_SIZE (INFRA_DEFAULT_FUNCTION_EXTRA_SIZE + (2 * sizeof(void*)))
#endif

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

    template<class State, class Event>
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
            infra::Function<bool(const Event&), SERVICES_FSM_FUNCTION_EXTRA_SIZE> guard;
            infra::Function<void(const Event&), SERVICES_FSM_FUNCTION_EXTRA_SIZE> execute;
        };

        template<std::size_t MaxTransitions, std::size_t QueueDepth>
        class WithStorage;

        TableStateMachine(infra::BoundedVector<Transition>& transitions, infra::BoundedDeque<Event>& queue);

        template<class From, class Ev, class To, class Guard = std::nullptr_t, class Action = std::nullptr_t>
        TableStateMachine& Add(Guard guard = nullptr, Action action = nullptr);
        template<class Ev, class To, class Guard = std::nullptr_t, class Action = std::nullptr_t>
        TableStateMachine& AddFromAny(Guard guard = nullptr, Action action = nullptr);
        template<class S, class Ev, class Action = std::nullptr_t, class Guard = std::nullptr_t>
        TableStateMachine& AddInternal(Action action = nullptr, Guard guard = nullptr);

        template<class Initial, class... Args>
        void Start(Args&&... args);
        bool Started() const;

        template<class Initial>
        ConsistencyError CheckConsistency() const;
        ConsistencyError CheckConsistency(StateId initial) const;
        template<class S, class E>
        bool HasTransition() const;
        bool HasTransition(StateId from, EventId event) const;
        infra::MemoryRange<const Transition> Transitions() const;

        const State& CurrentState() const override;
        DispatchResult Dispatch(const Event& event) override;
        bool Dispatching() const;

        template<class E, std::size_t ExtraSize = INFRA_DEFAULT_FUNCTION_EXTRA_SIZE>
        infra::Function<void(), ExtraSize> Completion();
        template<class E, std::size_t ExtraSize = INFRA_DEFAULT_FUNCTION_EXTRA_SIZE>
        infra::Function<void(), ExtraSize> Completion(E event);

    private:
        template<class From, class Ev, class To, class Action>
        void Execute(const Action& action, const Event& event);
        template<class S, class Ev, class Action>
        void ExecuteInternal(const Action& action, const Event& event);
        template<class From, class Ev, class Guard>
        void StoreGuard(Transition& transition, const Guard& guard);
        template<class From>
        auto& Source();
        template<class To, class From, class Ev, class Action>
        To Build(const Action& action, From& from, const Ev& event);
        void RunExit();
        void RunEntry();

        DispatchResult DispatchNow(const Event& event);
        const Transition* Select(const Event& event, bool& rowExists) const;
        const Transition* SelectFrom(std::optional<std::size_t> from, const Event& event, bool& rowExists) const;
        DispatchResult Reject(const Event& event, bool rowExists);
        void DrainQueue();
        void CompleteWith(const Event& event, uint32_t epochAtRequest);

        bool HasDuplicate() const;
        bool HasShadowed() const;
        bool HasUnreachable(StateId initial) const;
        static bool SamePair(const Transition& a, const Transition& b);

    private:
        infra::BoundedVector<Transition>& transitions;
        infra::BoundedDeque<Event>& queue;
        std::optional<State> currentState;
        uint32_t epoch{ 0 };
        bool dispatching{ false };
    };

    template<class State, class Event>
    template<std::size_t MaxTransitions, std::size_t QueueDepth>
    class TableStateMachine<State, Event>::WithStorage
        : public TableStateMachine<State, Event>
    {
    public:
        WithStorage();

    private:
        typename infra::BoundedVector<Transition>::template WithMaxSize<MaxTransitions> transitionsStorage;
        typename infra::BoundedDeque<Event>::template WithMaxSize<QueueDepth> queueStorage;
    };

    ////    Implementation    ////

    template<class State, class Event>
    TableStateMachine<State, Event>::TableStateMachine(infra::BoundedVector<Transition>& transitions, infra::BoundedDeque<Event>& queue)
        : transitions(transitions)
        , queue(queue)
    {}

    template<class State, class Event>
    template<std::size_t MaxTransitions, std::size_t QueueDepth>
    TableStateMachine<State, Event>::WithStorage<MaxTransitions, QueueDepth>::WithStorage()
        : TableStateMachine<State, Event>(transitionsStorage, queueStorage)
    {}

    template<class State, class Event>
    template<class From, class Ev, class To, class Guard, class Action>
    TableStateMachine<State, Event>& TableStateMachine<State, Event>::Add(Guard guard, Action action)
    {
        really_assert(!Started());
        transitions.emplace_back(Transition{ StateId::template Of<From>().Index(), EventId::template Of<Ev>().Index(), StateId::template Of<To>().Index(), false, false, nullptr, nullptr });
        this->template StoreGuard<From, Ev>(transitions.back(), guard);
        transitions.back().execute = [this, action](const Event& event)
        {
            this->template Execute<From, Ev, To>(action, event);
        };
        return *this;
    }

    template<class State, class Event>
    template<class Ev, class To, class Guard, class Action>
    TableStateMachine<State, Event>& TableStateMachine<State, Event>::AddFromAny(Guard guard, Action action)
    {
        really_assert(!Started());
        transitions.emplace_back(Transition{ std::nullopt, EventId::template Of<Ev>().Index(), StateId::template Of<To>().Index(), false, false, nullptr, nullptr });
        this->template StoreGuard<State, Ev>(transitions.back(), guard);
        transitions.back().execute = [this, action](const Event& event)
        {
            this->template Execute<State, Ev, To>(action, event);
        };
        return *this;
    }

    template<class State, class Event>
    template<class S, class Ev, class Action, class Guard>
    TableStateMachine<State, Event>& TableStateMachine<State, Event>::AddInternal(Action action, Guard guard)
    {
        really_assert(!Started());
        transitions.emplace_back(Transition{ StateId::template Of<S>().Index(), EventId::template Of<Ev>().Index(), StateId::template Of<S>().Index(), true, false, nullptr, nullptr });
        this->template StoreGuard<S, Ev>(transitions.back(), guard);
        transitions.back().execute = [this, action](const Event& event)
        {
            this->template ExecuteInternal<S, Ev>(action, event);
        };
        return *this;
    }

    template<class State, class Event>
    template<class Initial, class... Args>
    void TableStateMachine<State, Event>::Start(Args&&... args)
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
        DrainQueue();
        dispatching = false;
    }

    template<class State, class Event>
    bool TableStateMachine<State, Event>::Started() const
    {
        return currentState.has_value();
    }

    template<class State, class Event>
    template<class Initial>
    ConsistencyError TableStateMachine<State, Event>::CheckConsistency() const
    {
        return CheckConsistency(StateId::template Of<Initial>());
    }

    template<class State, class Event>
    ConsistencyError TableStateMachine<State, Event>::CheckConsistency(StateId initial) const
    {
        if (transitions.empty())
            return ConsistencyError::emptyTable;
        if (HasDuplicate())
            return ConsistencyError::duplicateTransition;
        if (HasShadowed())
            return ConsistencyError::shadowedTransition;
        if (HasUnreachable(initial))
            return ConsistencyError::unreachableState;

        return ConsistencyError::none;
    }

    template<class State, class Event>
    template<class S, class E>
    bool TableStateMachine<State, Event>::HasTransition() const
    {
        return HasTransition(StateId::template Of<S>(), EventId::template Of<E>());
    }

    template<class State, class Event>
    bool TableStateMachine<State, Event>::HasTransition(StateId from, EventId event) const
    {
        for (const auto& transition : transitions)
            if ((!transition.from || *transition.from == from.Index()) && transition.event == event.Index())
                return true;

        return false;
    }

    template<class State, class Event>
    infra::MemoryRange<const typename TableStateMachine<State, Event>::Transition> TableStateMachine<State, Event>::Transitions() const
    {
        return transitions.range();
    }

    template<class State, class Event>
    const State& TableStateMachine<State, Event>::CurrentState() const
    {
        really_assert(Started());
        return *currentState;
    }

    template<class State, class Event>
    DispatchResult TableStateMachine<State, Event>::Dispatch(const Event& event)
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

    template<class State, class Event>
    bool TableStateMachine<State, Event>::Dispatching() const
    {
        return dispatching;
    }

    template<class State, class Event>
    template<class E, std::size_t ExtraSize>
    infra::Function<void(), ExtraSize> TableStateMachine<State, Event>::Completion()
    {
        static_assert(std::is_default_constructible_v<E>, "Completion events without payload must be default constructible");
        return [this, epochAtRequest = epoch]()
        {
            CompleteWith(Event{ E{} }, epochAtRequest);
        };
    }

    template<class State, class Event>
    template<class E, std::size_t ExtraSize>
    infra::Function<void(), ExtraSize> TableStateMachine<State, Event>::Completion(E event)
    {
        return [this, epochAtRequest = epoch, event]()
        {
            CompleteWith(Event{ event }, epochAtRequest);
        };
    }

    template<class State, class Event>
    template<class From, class Ev, class To, class Action>
    void TableStateMachine<State, Event>::Execute(const Action& action, const Event& event)
    {
        RunExit();
        ++epoch;
        To next{ this->template Build<To>(action, this->template Source<From>(), std::get<Ev>(event)) };
        currentState->template emplace<To>(std::move(next));
        RunEntry();
    }

    template<class State, class Event>
    template<class S, class Ev, class Action>
    void TableStateMachine<State, Event>::ExecuteInternal(const Action& action, const Event& event)
    {
        if constexpr (!std::is_null_pointer_v<Action>)
        {
            static_assert(std::is_invocable_v<Action, S&, const Ev&>, "Internal action must be callable as void(S&, const Ev&)");
            action(std::get<S>(*currentState), std::get<Ev>(event));
        }
    }

    template<class State, class Event>
    template<class From, class Ev, class Guard>
    void TableStateMachine<State, Event>::StoreGuard(Transition& transition, const Guard& guard)
    {
        if constexpr (!std::is_null_pointer_v<Guard>)
        {
            static_assert(std::is_invocable_r_v<bool, Guard, const From&, const Ev&>, "Guard must be callable as bool(const From&, const Ev&)");
            transition.guarded = true;
            transition.guard = [this, guard](const Event& event)
            {
                return guard(std::as_const(this->template Source<From>()), std::get<Ev>(event));
            };
        }
    }

    template<class State, class Event>
    template<class From>
    auto& TableStateMachine<State, Event>::Source()
    {
        if constexpr (std::is_same_v<From, State>)
            return *currentState;
        else
            return std::get<From>(*currentState);
    }

    template<class State, class Event>
    template<class To, class From, class Ev, class Action>
    To TableStateMachine<State, Event>::Build(const Action& action, From& from, const Ev& event)
    {
        if constexpr (std::is_null_pointer_v<Action>)
        {
            static_assert(std::is_default_constructible_v<To>, "Target state must be default constructible when no action builds it");
            return To{};
        }
        else
        {
            static_assert(std::is_invocable_r_v<To, Action, From&, const Ev&>, "Action must be callable as To(From&, const Ev&)");
            return action(from, event);
        }
    }

    template<class State, class Event>
    void TableStateMachine<State, Event>::RunExit()
    {
        std::visit([](auto& state)
            {
                if constexpr (HasOnExit<std::remove_cvref_t<decltype(state)>>)
                    state.OnExit();
            },
            *currentState);
    }

    template<class State, class Event>
    void TableStateMachine<State, Event>::RunEntry()
    {
        std::visit([](auto& state)
            {
                if constexpr (HasOnEntry<std::remove_cvref_t<decltype(state)>>)
                    state.OnEntry();
            },
            *currentState);
    }

    template<class State, class Event>
    DispatchResult TableStateMachine<State, Event>::DispatchNow(const Event& event)
    {
        bool rowExists = false;
        const Transition* selected = Select(event, rowExists);

        if (selected == nullptr)
            return Reject(event, rowExists);

        auto from = this->CurrentStateId();
        selected->execute(event);

        if (!selected->internal)
            this->NotifyObservers([&](StateMachineObserver<State, Event>& observer)
                {
                    observer.StateChanged(from, event, this->CurrentStateId());
                });

        return DispatchResult::transitioned;
    }

    template<class State, class Event>
    const typename TableStateMachine<State, Event>::Transition* TableStateMachine<State, Event>::Select(const Event& event, bool& rowExists) const
    {
        const Transition* specific = SelectFrom(currentState->index(), event, rowExists);
        if (specific != nullptr)
            return specific;

        return SelectFrom(std::nullopt, event, rowExists);
    }

    template<class State, class Event>
    const typename TableStateMachine<State, Event>::Transition* TableStateMachine<State, Event>::SelectFrom(std::optional<std::size_t> from, const Event& event, bool& rowExists) const
    {
        for (const auto& transition : transitions)
            if (transition.from == from && transition.event == event.index())
            {
                rowExists = true;
                if (!transition.guarded || transition.guard(event))
                    return &transition;
            }

        return nullptr;
    }

    template<class State, class Event>
    DispatchResult TableStateMachine<State, Event>::Reject(const Event& event, bool rowExists)
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

    template<class State, class Event>
    void TableStateMachine<State, Event>::DrainQueue()
    {
        while (!queue.empty())
        {
            Event event{ queue.front() };
            queue.pop_front();
            DispatchNow(event);
        }
    }

    template<class State, class Event>
    void TableStateMachine<State, Event>::CompleteWith(const Event& event, uint32_t epochAtRequest)
    {
        if (epochAtRequest == epoch)
            Dispatch(event);
        else
            this->NotifyObservers([&](StateMachineObserver<State, Event>& observer)
                {
                    observer.EventDiscarded(this->CurrentStateId(), event);
                });
    }

    template<class State, class Event>
    bool TableStateMachine<State, Event>::HasDuplicate() const
    {
        for (auto first = transitions.begin(); first != transitions.end(); ++first)
            for (auto second = first + 1; second != transitions.end(); ++second)
                if (SamePair(*first, *second) && !first->guarded && !second->guarded)
                    return true;

        return false;
    }

    template<class State, class Event>
    bool TableStateMachine<State, Event>::HasShadowed() const
    {
        for (auto first = transitions.begin(); first != transitions.end(); ++first)
            for (auto second = first + 1; second != transitions.end(); ++second)
                if (SamePair(*first, *second) && !first->guarded)
                    return true;

        return false;
    }

    template<class State, class Event>
    bool TableStateMachine<State, Event>::HasUnreachable(StateId initial) const
    {
        std::array<bool, StateId::count> reached{};
        reached[initial.Index()] = true;

        bool changed = true;
        while (changed)
        {
            changed = false;
            for (const auto& transition : transitions)
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

    template<class State, class Event>
    bool TableStateMachine<State, Event>::SamePair(const Transition& a, const Transition& b)
    {
        return a.from == b.from && a.event == b.event;
    }
}

#endif
