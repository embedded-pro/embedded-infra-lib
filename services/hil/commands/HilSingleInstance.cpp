#include "services/hil/commands/HilSingleInstance.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    HilSingleInstance::HilSingleInstance(uint8_t instances)
        : instances(instances)
    {
        really_assert(instances != 0);
    }

    HilStatus HilSingleInstance::Parse(const HilArguments& arguments, uint8_t& index) const
    {
        uint32_t requested = 0;
        HilStatus status = HilStatus::done;
        arguments.NumberAt(0, requested, 0, instances - 1u, status);
        index = static_cast<uint8_t>(requested);
        return status;
    }

    HilStatus HilSingleInstance::Find(const HilArguments& arguments) const
    {
        uint8_t requested = 0;
        HilStatus status = Parse(arguments, requested);
        if (status != HilStatus::done)
            return status;

        if (index != requested || closing)
            return HilStatus::notOpen;

        return HilStatus::done;
    }

    bool HilSingleInstance::Occupied() const
    {
        return index.has_value();
    }

    uint8_t HilSingleInstance::Index() const
    {
        return *index;
    }

    void HilSingleInstance::Open(uint8_t index)
    {
        this->index = index;
    }

    void HilSingleInstance::StartClosing()
    {
        closing = true;
    }

    void HilSingleInstance::Closed()
    {
        index = std::nullopt;
        closing = false;
    }
}
