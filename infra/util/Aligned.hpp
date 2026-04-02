#ifndef INFRA_ALIGNED_HPP
#define INFRA_ALIGNED_HPP

#include "infra/util/MemoryRange.hpp"
#include <array>

namespace infra
{
    template<class As, class T>
    class Aligned
    {
    public:
        Aligned() = default;

        explicit Aligned(T value)
        {
            Copy(ReinterpretCastMemoryRange<As>(MakeRange(&value, &value + 1)), MakeRange(this->value));
        }

        T Value() const
        {
            T result;
            Copy(MakeRange(value), ReinterpretCastMemoryRange<As>(MakeRange(&result, &result + 1)));
            return result;
        }

        operator T() const
        {
            return Value();
        }

        bool operator==(Aligned other) const
        {
            return value == other.value;
        }

        bool operator==(T other) const
        {
            return Value() == other;
        }

        template<class U>
        bool operator==(U other) const
        {
            return Value() == static_cast<T>(other);
        }

    private:
        std::array<As, sizeof(T) / sizeof(As)> value;
    };
}

#endif
