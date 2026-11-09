#include "hal/null/CanNull.hpp"

namespace hal
{
    void CanNull::SendData(Id, const Message&, const infra::Function<void(bool success)>& actionOnCompletion)
    {
        actionOnCompletion(true);
    }

    void CanNull::ReceiveData(const infra::Function<void(Id id, const Message& data)>&)
    {}
}
