#include "hal/null/SerialCommunicationNull.hpp"

namespace hal
{
    void SerialCommunicationNull::SendData(infra::ConstByteRange, infra::Function<void()> actionOnCompletion)
    {
        actionOnCompletion();
    }

    void SerialCommunicationNull::ReceiveData(infra::Function<void(infra::ConstByteRange data)>)
    {}
}
