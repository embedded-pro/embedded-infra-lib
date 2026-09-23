#ifndef SERVICES_TRANSITION_RULES_HPP
#define SERVICES_TRANSITION_RULES_HPP

#include "services/fsm/AlternativeId.hpp"
#include <array>
#include <cstddef>

namespace services
{
    template<class State>
    class TransitionRules
    {
    public:
        using StateId = AlternativeId<State>;

        template<class From, class... To>
        constexpr TransitionRules Allow() const;
        template<class... To>
        constexpr TransitionRules AllowFromAny() const;
        template<class From, class... To>
        constexpr TransitionRules Forbid() const;
        template<class... S>
        constexpr TransitionRules Terminal() const;

        constexpr bool Restricted() const;
        constexpr bool IsTerminal(std::size_t state) const;
        constexpr bool IsExplicitlyAllowed(std::size_t from, std::size_t to) const;
        constexpr bool IsAllowedFromAny(std::size_t to) const;
        constexpr bool IsForbidden(std::size_t from, std::size_t to) const;
        constexpr bool IsAllowed(std::size_t from, std::size_t to) const;

    private:
        using Matrix = std::array<std::array<bool, StateId::count>, StateId::count>;

        Matrix allowed{};
        Matrix forbidden{};
        std::array<bool, StateId::count> allowedFromAny{};
        std::array<bool, StateId::count> terminal{};
        bool restricted{ false };
    };

    ////    Implementation    ////

    template<class State>
    template<class From, class... To>
    constexpr TransitionRules<State> TransitionRules<State>::Allow() const
    {
        auto result = *this;
        ((result.allowed[StateId::template Of<From>().Index()][StateId::template Of<To>().Index()] = true), ...);
        result.restricted = true;
        return result;
    }

    template<class State>
    template<class... To>
    constexpr TransitionRules<State> TransitionRules<State>::AllowFromAny() const
    {
        auto result = *this;
        ((result.allowedFromAny[StateId::template Of<To>().Index()] = true), ...);
        result.restricted = true;
        return result;
    }

    template<class State>
    template<class From, class... To>
    constexpr TransitionRules<State> TransitionRules<State>::Forbid() const
    {
        auto result = *this;
        ((result.forbidden[StateId::template Of<From>().Index()][StateId::template Of<To>().Index()] = true), ...);
        return result;
    }

    template<class State>
    template<class... S>
    constexpr TransitionRules<State> TransitionRules<State>::Terminal() const
    {
        auto result = *this;
        ((result.terminal[StateId::template Of<S>().Index()] = true), ...);
        return result;
    }

    template<class State>
    constexpr bool TransitionRules<State>::Restricted() const
    {
        return restricted;
    }

    template<class State>
    constexpr bool TransitionRules<State>::IsTerminal(std::size_t state) const
    {
        return terminal[state];
    }

    template<class State>
    constexpr bool TransitionRules<State>::IsExplicitlyAllowed(std::size_t from, std::size_t to) const
    {
        return allowed[from][to];
    }

    template<class State>
    constexpr bool TransitionRules<State>::IsAllowedFromAny(std::size_t to) const
    {
        return allowedFromAny[to];
    }

    template<class State>
    constexpr bool TransitionRules<State>::IsForbidden(std::size_t from, std::size_t to) const
    {
        return forbidden[from][to];
    }

    template<class State>
    constexpr bool TransitionRules<State>::IsAllowed(std::size_t from, std::size_t to) const
    {
        if (forbidden[from][to])
            return false;

        return !restricted || allowed[from][to] || allowedFromAny[to];
    }
}

#endif
