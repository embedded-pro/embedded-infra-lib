#ifndef INFRA_OPTIONAL_FOR_POLYMORPHIC_OBJECTS_HPP
#define INFRA_OPTIONAL_FOR_POLYMORPHIC_OBJECTS_HPP

#include "infra/util/StaticStorage.hpp"
#include <optional>
#include <type_traits>

namespace infra
{
    template<class T, std::size_t ExtraSize>
    class OptionalForPolymorphicObjects
    {
    public:
        OptionalForPolymorphicObjects() = default;

        OptionalForPolymorphicObjects(const OptionalForPolymorphicObjects& other) = delete;
        template<class Derived, std::size_t OtherExtraSize>
        OptionalForPolymorphicObjects(const OptionalForPolymorphicObjects<Derived, OtherExtraSize>& other, typename std::enable_if<std::is_base_of<T, Derived>::value>::type* = 0);
        OptionalForPolymorphicObjects(std::nullopt_t);
        template<class Derived, class... Args>
        OptionalForPolymorphicObjects(std::in_place_type_t<Derived>, Args&&... args);

        ~OptionalForPolymorphicObjects();

        OptionalForPolymorphicObjects& operator=(const OptionalForPolymorphicObjects& other) = delete;
        template<class Derived, std::size_t OtherExtraSize>
        typename std::enable_if<std::is_base_of<T, Derived>::value, OptionalForPolymorphicObjects&>::type
        operator=(const OptionalForPolymorphicObjects<Derived, OtherExtraSize>& other);
        OptionalForPolymorphicObjects& operator=(std::nullopt_t);
        OptionalForPolymorphicObjects& operator=(const T& value);
        OptionalForPolymorphicObjects& operator=(T&& value);

        template<class Derived, class... Args>
        void Emplace(Args&&... args);

        const T& operator*() const;
        T& operator*();
        const T* operator->() const;
        T* operator->();

        explicit operator bool() const;

        bool operator!() const;
        bool operator==(const OptionalForPolymorphicObjects& other) const;
        bool operator!=(const OptionalForPolymorphicObjects& other) const;

        friend bool operator==(const OptionalForPolymorphicObjects& x, std::nullopt_t)
        {
            return !x;
        }

        friend bool operator!=(const OptionalForPolymorphicObjects& x, std::nullopt_t y)
        {
            return !(x == y);
        }

        friend bool operator==(std::nullopt_t x, const OptionalForPolymorphicObjects& y)
        {
            return y == x;
        }

        friend bool operator!=(std::nullopt_t x, const OptionalForPolymorphicObjects& y)
        {
            return y != x;
        }

        friend bool operator==(const OptionalForPolymorphicObjects& x, const T& y)
        {
            return x && *x == y;
        }

        friend bool operator!=(const OptionalForPolymorphicObjects& x, const T& y)
        {
            return !(x == y);
        }

        friend bool operator==(const T& x, const OptionalForPolymorphicObjects& y)
        {
            return y == x;
        }

        friend bool operator!=(const T& x, const OptionalForPolymorphicObjects& y)
        {
            return y != x;
        }

    private:
        void Reset();

    private:
        bool initialized = false;
        StaticStorageForPolymorphicObjects<T, ExtraSize> data;
    };

    ////    Implementation    ////

    template<class T, std::size_t ExtraSize>
    template<class Derived, std::size_t OtherExtraSize>
    OptionalForPolymorphicObjects<T, ExtraSize>::OptionalForPolymorphicObjects(const OptionalForPolymorphicObjects<Derived, OtherExtraSize>& other, typename std::enable_if<std::is_base_of<T, Derived>::value>::type*)
    {
        if (other)
            Emplace<Derived>(*other);
    }

    template<class T, std::size_t ExtraSize>
    OptionalForPolymorphicObjects<T, ExtraSize>::OptionalForPolymorphicObjects(std::nullopt_t)
    {}

    template<class T, std::size_t ExtraSize>
    template<class Derived, class... Args>
    OptionalForPolymorphicObjects<T, ExtraSize>::OptionalForPolymorphicObjects(std::in_place_type_t<Derived>, Args&&... args)
    {
        Emplace<T>(std::forward<Args>(args)...);
    }

    template<class T, std::size_t ExtraSize>
    OptionalForPolymorphicObjects<T, ExtraSize>::~OptionalForPolymorphicObjects()
    {
        Reset();
    }

    template<class T, std::size_t ExtraSize>
    template<class Derived, std::size_t OtherExtraSize>
    typename std::enable_if<std::is_base_of<T, Derived>::value, OptionalForPolymorphicObjects<T, ExtraSize>&>::type
    OptionalForPolymorphicObjects<T, ExtraSize>::operator=(const OptionalForPolymorphicObjects<Derived, OtherExtraSize>& other)
    {
        Reset();
        if (other)
            Emplace<Derived>(*other);
        return *this;
    }

    template<class T, std::size_t ExtraSize>
    OptionalForPolymorphicObjects<T, ExtraSize>& OptionalForPolymorphicObjects<T, ExtraSize>::operator=(std::nullopt_t)
    {
        Reset();
        return *this;
    }

    template<class T, std::size_t ExtraSize>
    OptionalForPolymorphicObjects<T, ExtraSize>& OptionalForPolymorphicObjects<T, ExtraSize>::operator=(const T& value)
    {
        Reset();
        Emplace<T>(value);
        return *this;
    }

    template<class T, std::size_t ExtraSize>
    OptionalForPolymorphicObjects<T, ExtraSize>& OptionalForPolymorphicObjects<T, ExtraSize>::operator=(T&& value)
    {
        Reset();
        Emplace<T>(std::forward<T>(value));
        return *this;
    }

    template<class T, std::size_t ExtraSize>
    const T& OptionalForPolymorphicObjects<T, ExtraSize>::operator*() const
    {
        return *data;
    }

    template<class T, std::size_t ExtraSize>
    T& OptionalForPolymorphicObjects<T, ExtraSize>::operator*()
    {
        return *data;
    }

    template<class T, std::size_t ExtraSize>
    const T* OptionalForPolymorphicObjects<T, ExtraSize>::operator->() const
    {
        return &*data;
    }

    template<class T, std::size_t ExtraSize>
    T* OptionalForPolymorphicObjects<T, ExtraSize>::operator->()
    {
        return &*data;
    }

    template<class T, std::size_t ExtraSize>
    OptionalForPolymorphicObjects<T, ExtraSize>::operator bool() const
    {
        return initialized;
    }

    template<class T, std::size_t ExtraSize>
    bool OptionalForPolymorphicObjects<T, ExtraSize>::operator!() const
    {
        return !initialized;
    }

    template<class T, std::size_t ExtraSize>
    bool OptionalForPolymorphicObjects<T, ExtraSize>::operator==(const OptionalForPolymorphicObjects& other) const
    {
        if (initialized && other.initialized)
            return **this == *other;
        else
            return initialized == other.initialized;
    }

    template<class T, std::size_t ExtraSize>
    bool OptionalForPolymorphicObjects<T, ExtraSize>::operator!=(const OptionalForPolymorphicObjects& other) const
    {
        return !(*this == other);
    }

    template<class T, std::size_t ExtraSize>
    template<class Derived, class... Args>
    void OptionalForPolymorphicObjects<T, ExtraSize>::Emplace(Args&&... args)
    {
        Reset();
        initialized = true;
        data.template Construct<Derived>(std::forward<Args>(args)...);
    }

    template<class T, std::size_t ExtraSize>
    void OptionalForPolymorphicObjects<T, ExtraSize>::Reset()
    {
        if (initialized)
        {
            data.Destruct();
            initialized = false;
        }
    }
}

#endif
