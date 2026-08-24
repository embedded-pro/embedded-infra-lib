#include <cstdint>
#include <cstring>

// On ARMv7-M and ARMv8-M the compiler expands the __atomic builtins inline using
// LDREX/STREX, so libatomic outline calls are never emitted and defining these
// symbols would only add dead code. ARMv6-M (Cortex-M0/M0+) has no exclusive
// monitor instructions; there the compiler emits calls to the __atomic_* runtime,
// which is supplied below using interrupt-masking critical sections.
#if !defined(__ARM_FEATURE_LDREX)

namespace
{
    class CriticalSection
    {
    public:
        CriticalSection()
        {
            __asm volatile("mrs %0, primask"
                           : "=r"(primask));
            __asm volatile("cpsid i" ::: "memory");
        }

        CriticalSection(const CriticalSection&) = delete;
        CriticalSection& operator=(const CriticalSection&) = delete;

        ~CriticalSection()
        {
            __asm volatile("msr primask, %0" ::"r"(primask) : "memory");
        }

    private:
        uint32_t primask;
    };

    template<class T>
    T Load(const volatile void* mem)
    {
        CriticalSection criticalSection;
        return *static_cast<const volatile T*>(mem);
    }

    template<class T>
    void Store(volatile void* mem, T value)
    {
        CriticalSection criticalSection;
        *static_cast<volatile T*>(mem) = value;
    }

    template<class T>
    T Exchange(volatile void* mem, T value)
    {
        CriticalSection criticalSection;
        auto target = static_cast<volatile T*>(mem);
        T previous = *target;
        *target = value;
        return previous;
    }

    template<class T>
    bool CompareExchange(volatile void* mem, void* expected, T desired)
    {
        CriticalSection criticalSection;
        auto target = static_cast<volatile T*>(mem);
        auto comparand = static_cast<T*>(expected);

        if (*target != *comparand)
        {
            *comparand = *target;
            return false;
        }

        *target = desired;
        return true;
    }

    template<class T, class Operation>
    T FetchModify(volatile void* mem, T value, Operation operation)
    {
        CriticalSection criticalSection;
        auto target = static_cast<volatile T*>(mem);
        T previous = *target;
        *target = operation(previous, value);
        return previous;
    }
}

extern "C"
{
    [[gnu::weak, gnu::used]] unsigned char __atomic_load_1(const volatile void* mem, int)
    {
        return Load<unsigned char>(mem);
    }

    [[gnu::weak, gnu::used]] unsigned short __atomic_load_2(const volatile void* mem, int)
    {
        return Load<unsigned short>(mem);
    }

    [[gnu::weak, gnu::used]] unsigned int __atomic_load_4(const volatile void* mem, int)
    {
        return Load<unsigned int>(mem);
    }

    [[gnu::weak, gnu::used]] void __atomic_store_1(volatile void* mem, unsigned char value, int)
    {
        Store<unsigned char>(mem, value);
    }

    [[gnu::weak, gnu::used]] void __atomic_store_2(volatile void* mem, unsigned short value, int)
    {
        Store<unsigned short>(mem, value);
    }

    [[gnu::weak, gnu::used]] void __atomic_store_4(volatile void* mem, unsigned int value, int)
    {
        Store<unsigned int>(mem, value);
    }

    [[gnu::weak, gnu::used]] unsigned char __atomic_exchange_1(volatile void* mem, unsigned char value, int)
    {
        return Exchange<unsigned char>(mem, value);
    }

    [[gnu::weak, gnu::used]] unsigned short __atomic_exchange_2(volatile void* mem, unsigned short value, int)
    {
        return Exchange<unsigned short>(mem, value);
    }

    [[gnu::weak, gnu::used]] unsigned int __atomic_exchange_4(volatile void* mem, unsigned int value, int)
    {
        return Exchange<unsigned int>(mem, value);
    }

    [[gnu::weak, gnu::used]] bool __atomic_compare_exchange_1(volatile void* mem, void* expected, unsigned char desired, bool, int, int)
    {
        return CompareExchange<unsigned char>(mem, expected, desired);
    }

    [[gnu::weak, gnu::used]] bool __atomic_compare_exchange_2(volatile void* mem, void* expected, unsigned short desired, bool, int, int)
    {
        return CompareExchange<unsigned short>(mem, expected, desired);
    }

    [[gnu::weak, gnu::used]] bool __atomic_compare_exchange_4(volatile void* mem, void* expected, unsigned int desired, bool, int, int)
    {
        return CompareExchange<unsigned int>(mem, expected, desired);
    }

    [[gnu::weak, gnu::used]] unsigned int __atomic_fetch_add_4(volatile void* mem, unsigned int value, int)
    {
        return FetchModify<unsigned int>(mem, value, [](unsigned int previous, unsigned int operand)
            {
                return static_cast<unsigned int>(previous + operand);
            });
    }

    [[gnu::weak, gnu::used]] unsigned int __atomic_fetch_sub_4(volatile void* mem, unsigned int value, int)
    {
        return FetchModify<unsigned int>(mem, value, [](unsigned int previous, unsigned int operand)
            {
                return static_cast<unsigned int>(previous - operand);
            });
    }

    [[gnu::weak, gnu::used]] unsigned int __atomic_fetch_and_4(volatile void* mem, unsigned int value, int)
    {
        return FetchModify<unsigned int>(mem, value, [](unsigned int previous, unsigned int operand)
            {
                return static_cast<unsigned int>(previous & operand);
            });
    }

    [[gnu::weak, gnu::used]] unsigned int __atomic_fetch_or_4(volatile void* mem, unsigned int value, int)
    {
        return FetchModify<unsigned int>(mem, value, [](unsigned int previous, unsigned int operand)
            {
                return static_cast<unsigned int>(previous | operand);
            });
    }

    [[gnu::weak, gnu::used]] unsigned int __atomic_fetch_xor_4(volatile void* mem, unsigned int value, int)
    {
        return FetchModify<unsigned int>(mem, value, [](unsigned int previous, unsigned int operand)
            {
                return static_cast<unsigned int>(previous ^ operand);
            });
    }
}

#endif
