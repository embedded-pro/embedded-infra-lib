#ifndef INFRA_FUNCTION_HPP
#define INFRA_FUNCTION_HPP

//  Function is an object which can contain any callable function object, e.g. free function pointers
//  and lambda expressions. A Function is declared by providing the function signature and optionally
//  extra storage space for the function. For example, Function<void()> f; declares a function object
//  f that has void return and no parameters. Function<int(char x)> g; declares a function object g
//  that has an int return and one parameter of type char.
//  Examples:
//
//  Function<void()> f = [this]() { Trigger(); };
//  if (f)
//      f();        // call the lambda function which calls this->Trigger
//  f = nullptr;    // Clear the function
//
//  Since no heap is used, all data used by the function object must be stored inside Function itself.
//  The amount of data necessary is not known upfront, so the Function is parameterized with an
//  ExtraSize parameter to reserve storage. For example, this lambda expression requires no storage:
//  []() {} while this expression requires 8 bytes of extra storage: [this, &x]() { DoSomething(x); }.
//
//  Function<void()> f = [this, &x]() { DoSomething(x); }; // Compile error, too much storage is needed
//  Function<void(), 8> g = [this, &x]() { DoSomething(x); }; // Ok.

#include "infra/util/ByteRange.hpp"
#include "infra/util/ReallyAssert.hpp"
#include "infra/util/StaticStorage.hpp"
#include "infra/util/VariadicTemplates.hpp"
#include <cstddef>
#include <cstring>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#ifdef EMIL_HOST_BUILD
#include <ostream>
#endif

#ifndef INFRA_DEFAULT_FUNCTION_EXTRA_SIZE
#define INFRA_DEFAULT_FUNCTION_EXTRA_SIZE (2 * sizeof(void*))
#endif

#ifndef UTIL_FUNCTION_ALIGNMENT
#define UTIL_FUNCTION_ALIGNMENT void*
#endif

namespace infra
{
    template<class F, std::size_t ExtraSize = INFRA_DEFAULT_FUNCTION_EXTRA_SIZE>
    class Function;

    namespace detail
    {
        // The storage of a Function does not depend on its signature, so copying and destroying
        // Functions of different signatures shares the same code
        template<std::size_t ExtraSize>
        struct FunctionStorage
        {
            struct Lifetime
            {
                void (*destruct)(FunctionStorage& storage);
                void (*copyConstruct)(const FunctionStorage& from, FunctionStorage& to);
            };

            // A hand-crafted Virtual Method Table is used so that the destruct and copyConstruct functions
            // do not need to be implemented for trivial types (such as most lambdas), which saves space
            struct Table
            {
                const Lifetime* lifetime;
            };

            FunctionStorage() = default;
            template<class F>
            FunctionStorage(F&& f, const Table& table);
            FunctionStorage(const FunctionStorage& other);
            FunctionStorage& operator=(const FunctionStorage& other);
            ~FunctionStorage();

            bool Initialized() const;
            void Clear();
            void Swap(FunctionStorage& other) noexcept;

            template<class F>
            static void StaticDestruct(FunctionStorage& storage);
            template<class F>
            static void StaticCopyConstruct(const FunctionStorage& from, FunctionStorage& to);
            static const Table* Sentinel();

            template<class F>
            static constexpr Lifetime lifetimeOf{ &StaticDestruct<F>, &StaticCopyConstruct<F> };

            alignas(UTIL_FUNCTION_ALIGNMENT) mutable unsigned char data[ExtraSize];
            const Table* table{ Sentinel() };

        private:
            void CopyFrom(const FunctionStorage& other);
            void MoveTo(FunctionStorage& other);
        };

        template<class Signature, std::size_t ExtraSize>
        struct InvokerFunctions;

        template<std::size_t ExtraSize, class Result, class... Args>
        struct InvokerFunctions<Result(Args...), ExtraSize>
        {
            using StorageType = FunctionStorage<ExtraSize>;

            struct VirtualMethodTable
                : StorageType::Table
            {
                Result (*invoke)(const StorageType& storage, Args... args);
            };

            template<class F>
            static Result StaticInvoke(const StorageType& storage, Args... args);
            template<class F>
            static const VirtualMethodTable& StaticVirtualMethodTable();
            static const VirtualMethodTable& Table(const StorageType& storage);
        };

        extern const InvokerFunctions<void(), INFRA_DEFAULT_FUNCTION_EXTRA_SIZE>::VirtualMethodTable abortOnExecuteTable;
    }

    template<std::size_t ExtraSize, class Result, class... Args>
    class Function<Result(Args...), ExtraSize>
    {
    public:
        using ResultType = Result;
        using Arguments = List<Args...>;

    public:
        Function() = default;
        Function(std::nullptr_t);
        Function(const Function& other) = default;

        template<class F>
        Function(F f);

        ~Function() = default;

        Function& operator=(const Function& other) = default;
        Function& operator=(std::nullptr_t);

        explicit operator bool() const;

        ResultType operator()(Args... args) const;
        ResultType Invoke(const std::tuple<Args...>& args) const;

        void Swap(Function& other) noexcept;

    private:
        using Invokers = detail::InvokerFunctions<Result(Args...), ExtraSize>;
        using StorageType = typename Invokers::StorageType;

        StorageType storage;

        template<class F, std::size_t ExtraSize2>
        friend class Function;
    };

    class Execute
    {
    public:
        template<std::size_t ExtraSize>
        struct WithExtraSize
        {
            explicit WithExtraSize(Function<void(), ExtraSize> f);
        };

        explicit Execute(Function<void()> f);
    };

    class ExecuteOnDestruction
    {
    public:
        template<std::size_t ExtraSize>
        struct WithExtraSize
        {
            explicit WithExtraSize(Function<void(), ExtraSize> f);
            WithExtraSize(const WithExtraSize& other) = delete;
            WithExtraSize& operator=(const WithExtraSize& other) = delete;
            ~WithExtraSize();

        private:
            Function<void(), ExtraSize> f;
        };

        explicit ExecuteOnDestruction(Function<void()> f);
        ExecuteOnDestruction(const ExecuteOnDestruction& other) = delete;
        ExecuteOnDestruction& operator=(const ExecuteOnDestruction& other) = delete;
        ~ExecuteOnDestruction();

    private:
        Function<void()> f;
    };

    extern const infra::Function<void()> emptyFunction;

    template<std::size_t ExtraSize, class Result, class... Args>
    void swap(Function<Result(Args...), ExtraSize>& x, Function<Result(Args...), ExtraSize>& y) noexcept;

    template<std::size_t ExtraSize, class Result, class... Args>
    bool operator==(const Function<Result(Args...), ExtraSize>& f, std::nullptr_t);
    template<std::size_t ExtraSize, class Result, class... Args>
    bool operator==(std::nullptr_t, const Function<Result(Args...), ExtraSize>& f);

#ifdef EMIL_HOST_BUILD
    // gtest uses PrintTo to display the contents of Function
    template<class... Args>
    struct PrintParameterNames;

    template<>
    struct PrintParameterNames<>
    {
        explicit PrintParameterNames(std::ostream*)
        {}
    };

    template<class Arg>
    struct PrintParameterNames<Arg>
    {
        explicit PrintParameterNames(std::ostream* os)
        {
            *os << typeid(Arg).name();
        }
    };

    template<class Arg, class Arg2, class... Args>
    struct PrintParameterNames<Arg, Arg2, Args...>
    {
        explicit PrintParameterNames(std::ostream* os)
        {
            *os << typeid(Arg).name() << ", ";

            PrintParameterNames<Arg2, Args...> print(os);
        }
    };

    template<class R, class... Args>
    void PrintTo(Function<R(Args...)>, std::ostream* os)
    {
        *os << "Function<" << typeid(R).name() << "(";

        PrintParameterNames<Args...> print(os);

        *os << ")>";
    }
#endif

    //// Implementation ////

    namespace detail
    {
        template<std::size_t ExtraSize>
        template<class F>
        FunctionStorage<ExtraSize>::FunctionStorage(F&& f, const Table& table)
            : table(&table)
        {
            using Stored = std::decay_t<F>;

            static_assert(sizeof(Stored) <= ExtraSize, "Not enough static storage available for construction of derived type");
            static_assert(std::alignment_of_v<Stored> <= sizeof(UTIL_FUNCTION_ALIGNMENT), "Alignment of U is larger than alignment of this function");

            new (&data) Stored(std::forward<F>(f));
            std::memset(reinterpret_cast<char*>(&data) + sizeof(Stored), 0, ExtraSize - sizeof(Stored));
        }

        template<std::size_t ExtraSize>
        FunctionStorage<ExtraSize>::FunctionStorage(const FunctionStorage& other)
        {
            CopyFrom(other);
        }

        template<std::size_t ExtraSize>
        FunctionStorage<ExtraSize>& FunctionStorage<ExtraSize>::operator=(const FunctionStorage& other)
        {
            if (this != &other)
            {
                Clear();
                CopyFrom(other);
            }

            return *this;
        }

        template<std::size_t ExtraSize>
        FunctionStorage<ExtraSize>::~FunctionStorage()
        {
            Clear();
        }

        template<std::size_t ExtraSize>
        bool FunctionStorage<ExtraSize>::Initialized() const
        {
            return table != Sentinel();
        }

        template<std::size_t ExtraSize>
        void FunctionStorage<ExtraSize>::Clear()
        {
            if (Initialized())
            {
                if (table->lifetime != nullptr)
                    table->lifetime->destruct(*this);

                table = Sentinel();
            }
        }

        template<std::size_t ExtraSize>
        void FunctionStorage<ExtraSize>::Swap(FunctionStorage& other) noexcept
        {
            if (Initialized() && other.Initialized())
            {
                FunctionStorage temp(*this);
                *this = other;
                other = temp;
            }
            else if (Initialized())
                MoveTo(other);
            else if (other.Initialized())
                other.MoveTo(*this);
        }

        template<std::size_t ExtraSize>
        template<class F>
        void FunctionStorage<ExtraSize>::StaticDestruct(FunctionStorage& storage)
        {
            std::launder(reinterpret_cast<F*>(&storage.data))->~F(); //NOSONAR
        }

        template<std::size_t ExtraSize>
        template<class F>
        void FunctionStorage<ExtraSize>::StaticCopyConstruct(const FunctionStorage& from, FunctionStorage& to)
        {
            new (&to.data) F(*std::launder(reinterpret_cast<const F*>(&from.data)));
            std::memset(reinterpret_cast<char*>(&to.data) + sizeof(F), 0, ExtraSize - sizeof(F));
            to.table = from.table;
        }

        template<std::size_t ExtraSize>
        const typename FunctionStorage<ExtraSize>::Table* FunctionStorage<ExtraSize>::Sentinel()
        {
            const FunctionStorage<INFRA_DEFAULT_FUNCTION_EXTRA_SIZE>::Table& sentinel = abortOnExecuteTable;
            return reinterpret_cast<const Table*>(&sentinel); //NOSONAR
        }

        template<std::size_t ExtraSize>
        void FunctionStorage<ExtraSize>::CopyFrom(const FunctionStorage& other)
        {
            if (other.Initialized())
            {
                if (other.table->lifetime != nullptr)
                    other.table->lifetime->copyConstruct(other, *this);
                else
                    Copy(MakeByteRange(other), MakeByteRange(*this));
            }
        }

        template<std::size_t ExtraSize>
        void FunctionStorage<ExtraSize>::MoveTo(FunctionStorage& other)
        {
            other.CopyFrom(*this);
            Clear();
        }

        template<std::size_t ExtraSize, class Result, class... Args>
        template<class F>
        Result InvokerFunctions<Result(Args...), ExtraSize>::StaticInvoke(const StorageType& storage, Args... args)
        {
            return (*std::launder(reinterpret_cast<F*>(&storage.data)))(std::forward<Args>(args)...); //NOSONAR
        }

        template<std::size_t ExtraSize, class Result, class... Args>
        template<class F>
        const typename InvokerFunctions<Result(Args...), ExtraSize>::VirtualMethodTable& InvokerFunctions<Result(Args...), ExtraSize>::StaticVirtualMethodTable()
        {
            if constexpr (std::is_trivially_copy_constructible_v<F> && std::is_trivially_destructible_v<F>)
            {
                static const VirtualMethodTable table = { { nullptr }, &StaticInvoke<F> };
                return table;
            }
            else
            {
                static const VirtualMethodTable table = { { &StorageType::template lifetimeOf<F> }, &StaticInvoke<F> };
                return table;
            }
        }

        template<std::size_t ExtraSize, class Result, class... Args>
        const typename InvokerFunctions<Result(Args...), ExtraSize>::VirtualMethodTable& InvokerFunctions<Result(Args...), ExtraSize>::Table(const StorageType& storage)
        {
            return *static_cast<const VirtualMethodTable*>(storage.table);
        }

        template<class ResultType, class... Args>
        struct TupleInvokeHelper
        {
            template<class Storage, class FunctionPointer, std::size_t... I>
            static ResultType Invoke(const Storage& storage, FunctionPointer invoke, const std::tuple<Args...>& args, std::index_sequence<I...>)
            {
                return invoke(storage, std::get<I>(args)...);
            }
        };
    }

    template<std::size_t ExtraSize, class Result, class... Args>
    Function<Result(Args...), ExtraSize>::Function(std::nullptr_t)
    {}

    template<std::size_t ExtraSize, class Result, class... Args>
    template<class F>
    Function<Result(Args...), ExtraSize>::Function(F f)
        : storage(std::move(f), Invokers::template StaticVirtualMethodTable<F>())
    {}

    template<std::size_t ExtraSize, class Result, class... Args>
    Function<Result(Args...), ExtraSize>& Function<Result(Args...), ExtraSize>::operator=(std::nullptr_t)
    {
        storage.Clear();
        return *this;
    }

    template<std::size_t ExtraSize, class Result, class... Args>
    Function<Result(Args...), ExtraSize>::operator bool() const
    {
        return storage.Initialized();
    }

    template<std::size_t ExtraSize, class Result, class... Args>
    typename Function<Result(Args...), ExtraSize>::ResultType Function<Result(Args...), ExtraSize>::operator()(Args... args) const
    {
        return Invokers::Table(storage).invoke(storage, std::forward<Args>(args)...); //NOSONAR
    }

    template<std::size_t ExtraSize, class Result, class... Args>
    typename Function<Result(Args...), ExtraSize>::ResultType Function<Result(Args...), ExtraSize>::Invoke(const std::tuple<Args...>& args) const
    {
        return detail::TupleInvokeHelper<ResultType, Args...>::Invoke(storage, Invokers::Table(storage).invoke, args, std::make_index_sequence<sizeof...(Args)>{});
    }

    template<std::size_t ExtraSize, class Result, class... Args>
    void Function<Result(Args...), ExtraSize>::Swap(Function& other) noexcept
    {
        storage.Swap(other.storage);
    }

    template<std::size_t ExtraSize, class Result, class... Args>
    void swap(Function<Result(Args...), ExtraSize>& x, Function<Result(Args...), ExtraSize>& y) noexcept
    {
        x.Swap(y);
    }

    template<std::size_t ExtraSize, class Result, class... Args>
    bool operator==(const Function<Result(Args...), ExtraSize>& f, std::nullptr_t)
    {
        return !f;
    }

    template<std::size_t ExtraSize, class Result, class... Args>
    bool operator==(std::nullptr_t, const Function<Result(Args...), ExtraSize>& f)
    {
        return !f;
    }

    template<std::size_t ExtraSize>
    Execute::WithExtraSize<ExtraSize>::WithExtraSize(Function<void(), ExtraSize> f)
    {
        f();
    }

    template<std::size_t ExtraSize>
    ExecuteOnDestruction::WithExtraSize<ExtraSize>::WithExtraSize(Function<void(), ExtraSize> f)
        : f(f)
    {}

    template<std::size_t ExtraSize>
    ExecuteOnDestruction::WithExtraSize<ExtraSize>::~WithExtraSize()
    {
        f();
    }
}

#endif
