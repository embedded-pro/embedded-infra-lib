#ifndef INFRA_VARIANT_HPP
#define INFRA_VARIANT_HPP

#include "infra/util/Optional.hpp"
#include "infra/util/VariantDetail.hpp"
#include <type_traits>
#include <variant>

#ifdef _MSC_VER
#define NOEXCEPT_SPECIFICATION(x) // VS2022 does not handle nothrow specifications well in c++20 mode
#else
#define NOEXCEPT_SPECIFICATION(x) noexcept(x)
#endif

namespace infra
{
    struct AtIndex
    {};

    const AtIndex atIndex;

    template<class... T>
    class Variant
    {
    public:
        static const std::size_t size = sizeof...(T);

        Variant() = default;
        Variant(const Variant& other) = default;
        Variant(Variant&& other) NOEXCEPT_SPECIFICATION((std::is_nothrow_move_constructible_v<T> && ...)) = default;
        template<class... T2>
        Variant(const Variant<T2...>& other);
        template<class U>
        Variant(const U& v, typename std::enable_if<ExistsInTypeList<U, T...>::value>::type* = 0);
        template<class U, class... Args>
        explicit Variant(InPlaceType<U>, Args&&... args);
        template<class... Args>
        Variant(AtIndex, std::size_t index, Args&&... args);

        Variant& operator=(const Variant& other);
        Variant& operator=(Variant&& other) NOEXCEPT_SPECIFICATION((std::is_nothrow_move_assignable_v<T> && ...) && (std::is_nothrow_move_constructible_v<T> && ...));
        template<class... T2>
        Variant& operator=(const Variant<T2...>& other);
        template<class U>
        Variant& operator=(const U& v);

        ~Variant() = default;

        template<class U, class... Args>
        U& Emplace(Args&&... args);

        template<class U>
        const U& Get() const;
        template<class U>
        U& Get();

        template<std::size_t Index>
        const typename TypeAtIndex<Index, T...>::Type& GetAtIndex() const;
        template<std::size_t Index>
        typename TypeAtIndex<Index, T...>::Type& GetAtIndex();

        std::size_t Which() const;
        template<class U>
        bool Is() const;

        bool operator==(const Variant& other) const;
        bool operator!=(const Variant& other) const;
        bool operator<(const Variant& other) const;
        bool operator>(const Variant& other) const;
        bool operator<=(const Variant& other) const;
        bool operator>=(const Variant& other) const;

        template<class U>
        typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type operator==(const U& other) const;
        template<class U>
        typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type operator!=(const U& other) const;
        template<class U>
        typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type operator<(const U& other) const;
        template<class U>
        typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type operator>(const U& other) const;
        template<class U>
        typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type operator<=(const U& other) const;
        template<class U>
        typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type operator>=(const U& other) const;

        template<class U, class... Args>
        U& ConstructInEmptyVariant(Args&&... args);

        template<class... Args>
        void ConstructByIndexInEmptyVariant(std::size_t index, Args&&... args);

        void Destruct();

    private:
        std::variant<T...> storage;

        template<class... T2>
        friend struct detail::CopyConstructVisitor;
        template<class... T2>
        friend struct detail::MoveConstructVisitor;
    };

    template<class... T>
    struct MakeVariantOver;

    template<class Visitor, class Variant>
    typename Visitor::ResultType ApplyVisitor(Visitor& visitor, Variant& variant);
    template<class Visitor, class Variant>
    typename Visitor::ResultType ApplyVisitor(Visitor& visitor, Variant& variant1, Variant& variant2);
    template<class Visitor, class Variant>
    typename Visitor::ResultType ApplySameTypeVisitor(Visitor& visitor, Variant& variant1, Variant& variant2);

    ////    Implementation    ////

    template<class... T>
    template<class... T2>
    Variant<T...>::Variant(const Variant<T2...>& other)
    {
        detail::CopyConstructVisitor<T...> visitor(*this);
        ApplyVisitor(visitor, other);
    }

    template<class... T>
    template<class U>
    Variant<T...>::Variant(const U& v, typename std::enable_if<ExistsInTypeList<U, T...>::value>::type*)
        : storage(v)
    {}

    template<class... T>
    template<class U, class... Args>
    Variant<T...>::Variant(InPlaceType<U>, Args&&... args)
        : storage(std::in_place_type<U>, std::forward<Args>(args)...)
    {}

    template<class... T>
    template<class... Args>
    Variant<T...>::Variant(AtIndex, std::size_t index, Args&&... args)
    {
        ConstructByIndexInEmptyVariant(index, std::forward<Args>(args)...);
    }

    template<class... T>
    template<class... T2>
    Variant<T...>& Variant<T...>::operator=(const Variant<T2...>& other)
    {
        detail::CopyAssignVisitor<T...> visitor(*this);
        ApplyVisitor(visitor, other);
        return *this;
    }

    template<class... T>
    Variant<T...>& Variant<T...>::operator=(const Variant<T...>& other)
    {
        if (this != &other)
        {
            detail::CopyAssignVisitor<T...> visitor(*this);
            ApplyVisitor(visitor, other);
        }
        return *this;
    }

    template<class... T>
    Variant<T...>& Variant<T...>::operator=(Variant<T...>&& other) NOEXCEPT_SPECIFICATION((std::is_nothrow_move_assignable_v<T> && ...) && (std::is_nothrow_move_constructible_v<T> && ...))
    {
        if (this != &other)
        {
            detail::MoveAssignVisitor<T...> visitor(*this);
            ApplyVisitor(visitor, other);
        }
        return *this;
    }

    template<class... T>
    template<class U>
    Variant<T...>& Variant<T...>::operator=(const U& v)
    {
        storage.template emplace<U>(v);
        return *this;
    }

    template<class... T>
    template<class U, class... Args>
    U& Variant<T...>::Emplace(Args&&... args)
    {
        return storage.template emplace<U>(std::forward<Args>(args)...);
    }

    template<class... T>
    template<class U>
    const U& Variant<T...>::Get() const
    {
        really_assert((storage.index() == IndexInTypeList<U, T...>::value));
        return std::get<U>(storage);
    }

    template<class... T>
    template<class U>
    U& Variant<T...>::Get()
    {
        really_assert((storage.index() == IndexInTypeList<U, T...>::value));
        return std::get<U>(storage);
    }

    template<class... T>
    template<std::size_t Index>
    const typename TypeAtIndex<Index, T...>::Type& Variant<T...>::GetAtIndex() const
    {
        return std::get<Index>(storage);
    }

    template<class... T>
    template<std::size_t Index>
    typename TypeAtIndex<Index, T...>::Type& Variant<T...>::GetAtIndex()
    {
        return std::get<Index>(storage);
    }

    template<class... T>
    std::size_t Variant<T...>::Which() const
    {
        return storage.index();
    }

    template<class... T>
    template<class U>
    bool Variant<T...>::Is() const
    {
        return std::holds_alternative<U>(storage);
    }

    template<class... T>
    bool Variant<T...>::operator==(const Variant& other) const
    {
        if (Which() != other.Which())
            return false;

        detail::EqualVisitor visitor;
        return ApplySameTypeVisitor(visitor, *this, other);
    }

    template<class... T>
    bool Variant<T...>::operator!=(const Variant& other) const
    {
        return !(*this == other);
    }

    template<class... T>
    bool Variant<T...>::operator<(const Variant& other) const
    {
        if (Which() != other.Which())
            return Which() < other.Which();

        detail::LessThanVisitor visitor;
        return ApplySameTypeVisitor(visitor, *this, other);
    }

    template<class... T>
    bool Variant<T...>::operator>(const Variant& other) const
    {
        return other < *this;
    }

    template<class... T>
    bool Variant<T...>::operator<=(const Variant& other) const
    {
        return !(other < *this);
    }

    template<class... T>
    bool Variant<T...>::operator>=(const Variant& other) const
    {
        return !(*this < other);
    }

    template<class... T>
    template<class U>
    typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type Variant<T...>::operator==(const U& other) const
    {
        if (Which() != IndexInTypeList<U, T...>::value)
            return false;

        return GetAtIndex<IndexInTypeList<U, T...>::value>() == other;
    }

    template<class... T>
    template<class U>
    typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type Variant<T...>::operator!=(const U& other) const
    {
        return !(*this == other);
    }

    template<class... T>
    template<class U>
    typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type Variant<T...>::operator<(const U& other) const
    {
        if (Which() != IndexInTypeList<U, T...>::value)
            return Which() < IndexInTypeList<U, T...>::value;

        return GetAtIndex<IndexInTypeList<U, T...>::value>() < other;
    }

    template<class... T>
    template<class U>
    typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type Variant<T...>::operator>(const U& other) const
    {
        if (Which() != IndexInTypeList<U, T...>::value)
            return Which() > IndexInTypeList<U, T...>::value;

        return other < GetAtIndex<IndexInTypeList<U, T...>::value>();
    }

    template<class... T>
    template<class U>
    typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type Variant<T...>::operator<=(const U& other) const
    {
        return !(*this > other);
    }

    template<class... T>
    template<class U>
    typename std::enable_if<ExistsInTypeList<U, T...>::value, bool>::type Variant<T...>::operator>=(const U& other) const
    {
        return !(*this < other);
    }

    template<class... T>
    template<class U, class... Args>
    U& Variant<T...>::ConstructInEmptyVariant(Args&&... args)
    {
        return storage.template emplace<U>(std::forward<Args>(args)...);
    }

    template<class... T>
    template<class... Args>
    void Variant<T...>::ConstructByIndexInEmptyVariant(std::size_t index, Args&&... args)
    {
        detail::ConstructAtIndexHelper<T...>::Construct(*this, index, std::forward<Args>(args)...);
    }

    template<class... T>
    void Variant<T...>::Destruct()
    {}

    template<class Visitor, class Variant>
    typename Visitor::ResultType ApplyVisitor(Visitor& visitor, Variant& variant)
    {
        detail::ApplyVisitorHelper<0, Visitor, Variant> helper;
        return helper(visitor, variant);
    }

    template<class Visitor, class Variant>
    typename Visitor::ResultType ApplyVisitor(Visitor& visitor, Variant& variant1, Variant& variant2)
    {
        detail::ApplyVisitorHelper2<0, Visitor, Variant> helper;
        return helper(visitor, variant1, variant2);
    }

    template<class Visitor, class Variant>
    typename Visitor::ResultType ApplySameTypeVisitor(Visitor& visitor, Variant& variant1, Variant& variant2)
    {
        really_assert(variant1.Which() == variant2.Which());
        detail::ApplySameTypeVisitorHelper<0, Visitor, Variant> helper;
        return helper(visitor, variant1, variant2);
    }

    template<class... T>
    struct MakeVariantOver<List<T...>>
    {
        using Type = Variant<T...>;
    };

}

#endif
