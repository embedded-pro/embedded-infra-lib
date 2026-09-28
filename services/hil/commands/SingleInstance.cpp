#include "services/hil/commands/SingleInstance.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services::hil
{
    SingleInstance::SingleInstance(uint8_t instances)
        : instances(instances)
    {
        really_assert(instances != 0);
    }

    Status SingleInstance::Parse(const Arguments& arguments, uint8_t& index) const
    {
        uint32_t requested = 0;
        Status status = Status::done;
        arguments.NumberAt(0, requested, 0, instances - 1u, status);
        index = static_cast<uint8_t>(requested);
        return status;
    }

    Status SingleInstance::Find(const Arguments& arguments) const
    {
        uint8_t requested = 0;
        Status status = Parse(arguments, requested);
        if (status != Status::done)
            return status;

        if (index != requested || closing)
            return Status::notOpen;

        return Status::done;
    }

    bool SingleInstance::Occupied() const
    {
        return index.has_value();
    }

    uint8_t SingleInstance::Index() const
    {
        return *index;
    }

    void SingleInstance::Open(uint8_t index)
    {
        this->index = index;
    }

    void SingleInstance::StartClosing()
    {
        closing = true;
    }

    void SingleInstance::Closed()
    {
        index = std::nullopt;
        closing = false;
    }
}
